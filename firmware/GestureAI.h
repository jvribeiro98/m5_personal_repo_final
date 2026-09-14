#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <cstdint>
#include <cmath>
#include <algorithm>
#endif
#include <cmath>

// ============================================================
// GESTURE AI - Reconhecimento Inercial 3D de Gestos no Ar
// M5StickC Plus 2 (MPU6886 IMU)
// ============================================================

enum GestureType : uint8_t {
    GESTURE_NONE = 0,
    GESTURE_CIRCLE,
    GESTURE_CHECK_V,
    GESTURE_ZIGZAG,
    GESTURE_TRIANGLE,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_THRUST,
    GESTURE_COUNT
};

struct GesturePoint {
    float x;
    float y;
};

struct GestureResult {
    GestureType type;
    float confidence;      // 0.0 a 100.0%
    const char* name;
    const char* action;
    const char* symbol;
    uint16_t color;
};

class GestureRecognizer {
public:
    static constexpr int MAX_RAW_POINTS = 128;
    static constexpr int N_POINTS = 32;

    GestureRecognizer() : _rawCount(0), _recording(false), _lastSampleMs(0), _accumX(0.0f), _accumY(0.0f) {}

    void startRecording() {
        _rawCount = 0;
        _accumX = 0.0f;
        _accumY = 0.0f;
        _recording = true;
        _lastSampleMs = millis();
        _maxThrustG = 0.0f;
    }

    bool isRecording() const {
        return _recording;
    }

    int getRawPointCount() const {
        return _rawCount;
    }

    const GesturePoint* getRawPoints() const {
        return _rawPoints;
    }

    // Amostragem contínua enquanto o botão está pressionado (~50 Hz)
    void sample(float gx, float gy, float gz, float ax, float ay, float az) {
        if (!_recording) return;
        uint32_t now = millis();
        if (now - _lastSampleMs < 18) return; // Limita a aprox 50Hz
        float dt = (now - _lastSampleMs) / 1000.0f;
        if (dt > 0.1f) dt = 0.02f;
        _lastSampleMs = now;

        // Movimento angular da ponta do M5Stick:
        // Yaw (gz) -> deslocamento X horizontal
        // Pitch (gx) -> deslocamento Y vertical
        float vx = -gz * dt;
        float vy = -gx * dt;

        _accumX += vx;
        _accumY += vy;

        // Monitora pico de estocada (aceleração para frente no eixo Y/Z)
        float thrust = fabsf(ay) + fabsf(az);
        if (thrust > _maxThrustG) _maxThrustG = thrust;

        if (_rawCount < MAX_RAW_POINTS) {
            _rawPoints[_rawCount].x = _accumX;
            _rawPoints[_rawCount].y = _accumY;
            _rawCount++;
        }
    }

    GestureResult finishAndClassify() {
        _recording = false;
        GestureResult res;
        res.type = GESTURE_NONE;
        res.confidence = 0.0f;
        res.name = "NAO RECONHECIDO";
        res.action = "Tente novamente";
        res.symbol = "?";
        res.color = 0x9CD3; // UI_MUTED

        if (_rawCount < 12) {
            res.action = "Movimento muito curto";
            return res;
        }

        // 1. Checa se foi uma estocada direta (Punch / Thrust para frente)
        if (_maxThrustG > 2.8f && getPathLength(_rawPoints, _rawCount) < 0.15f) {
            res.type = GESTURE_THRUST;
            res.confidence = 92.0f;
            res.name = "ESTOCADA";
            res.action = "CONFIRMAR / SELECIONAR";
            res.symbol = "->";
            res.color = 0xFFFF; // UI_TEXT
            return res;
        }

        // 2. Resample para N_POINTS
        resample(_rawPoints, _rawCount, _normPoints, N_POINTS);

        // 3. Métricas da trajetória
        float totalLength = getPathLength(_normPoints, N_POINTS);
        if (totalLength < 0.005f) {
            res.action = "Sem movimento suficiente";
            return res;
        }

        float dx = _normPoints[N_POINTS - 1].x - _normPoints[0].x;
        float dy = _normPoints[N_POINTS - 1].y - _normPoints[0].y;
        float straightDistance = sqrtf(dx * dx + dy * dy);
        float closureRatio = straightDistance / totalLength; // Perto de 1 = linha reta; Perto de 0 = curva fechada/círculo

        // 4. Detecção de Swipes rápidos (linhas retas direcionais)
        if (closureRatio > 0.82f) {
            if (fabsf(dx) > fabsf(dy) * 1.5f) {
                if (dx > 0) {
                    res.type = GESTURE_SWIPE_RIGHT;
                    res.confidence = 94.0f;
                    res.name = "SWIPE DIREITA";
                    res.action = "TV CANAL +";
                    res.symbol = ">>";
                    res.color = 0x5F37; // Verde agua
                    return res;
                } else {
                    res.type = GESTURE_SWIPE_LEFT;
                    res.confidence = 94.0f;
                    res.name = "SWIPE ESQUERDA";
                    res.action = "TV CANAL -";
                    res.symbol = "<<";
                    res.color = 0x5F37;
                    return res;
                }
            } else if (fabsf(dy) > fabsf(dx) * 1.5f) {
                if (dy < 0) { // Na tela Y cresce para baixo, dy negativo = cima
                    res.type = GESTURE_SWIPE_UP;
                    res.confidence = 95.0f;
                    res.name = "SWIPE CIMA";
                    res.action = "VOLUME + / TEMP +";
                    res.symbol = "^";
                    res.color = 0x5F37;
                    return res;
                } else {
                    res.type = GESTURE_SWIPE_DOWN;
                    res.confidence = 95.0f;
                    res.name = "SWIPE BAIXO";
                    res.action = "VOLUME - / TEMP -";
                    res.symbol = "v";
                    res.color = 0xF800; // UI_RED
                    return res;
                }
            }
        }

        // 5. Normaliza escala e centroide para $1 Recognizer
        scaleTo(_normPoints, N_POINTS, 100.0f);
        translateToOrigin(_normPoints, N_POINTS);

        // 6. Compara com os templates canônicos
        float bestDist = 999999.0f;
        GestureType bestType = GESTURE_NONE;

        // Template Círculo (horário e anti-horário)
        float dCircleCW = matchTemplate(_normPoints, getCircleTemplate(true));
        float dCircleCCW = matchTemplate(_normPoints, getCircleTemplate(false));
        float dCircle = std::min(dCircleCW, dCircleCCW);
        if (closureRatio < 0.45f && dCircle < bestDist) {
            bestDist = dCircle;
            bestType = GESTURE_CIRCLE;
        }

        // Template Letra V (Check)
        float dV = matchTemplate(_normPoints, getVTemplate());
        if (dV < bestDist) {
            bestDist = dV;
            bestType = GESTURE_CHECK_V;
        }

        // Template Letra Z (Raio)
        float dZ = matchTemplate(_normPoints, getZTemplate());
        if (dZ < bestDist) {
            bestDist = dZ;
            bestType = GESTURE_ZIGZAG;
        }

        // Template Triângulo
        float dTri = matchTemplate(_normPoints, getTriangleTemplate());
        if (closureRatio < 0.50f && dTri < bestDist) {
            bestDist = dTri;
            bestType = GESTURE_TRIANGLE;
        }

        // Converte distância em pontuação de confiança (0 a 100%)
        // Diagonal de uma caixa de 100x100 = ~141.4. Meia diagonal = ~70.7
        float maxD = 70.7f;
        float score = (1.0f - (bestDist / maxD)) * 100.0f;
        if (score < 0.0f) score = 0.0f;
        if (score > 100.0f) score = 100.0f;

        if (score >= 68.0f && bestType != GESTURE_NONE) {
            res.type = bestType;
            res.confidence = score;
            fillMetadata(res);
        } else {
            res.type = GESTURE_NONE;
            res.confidence = score;
            res.name = "NAO RECONHECIDO";
            res.action = "Desenhe novamente";
            res.symbol = "?";
            res.color = 0x9CD3;
        }

        return res;
    }

private:
    GesturePoint _rawPoints[MAX_RAW_POINTS];
    GesturePoint _normPoints[N_POINTS];
    int _rawCount;
    bool _recording;
    uint32_t _lastSampleMs;
    float _accumX;
    float _accumY;
    float _maxThrustG;

    static float distance(const GesturePoint& a, const GesturePoint& b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }

    static float getPathLength(const GesturePoint* pts, int count) {
        float len = 0.0f;
        for (int i = 1; i < count; ++i) {
            len += distance(pts[i - 1], pts[i]);
        }
        return len;
    }

    static void resample(const GesturePoint* src, int srcCount, GesturePoint* dst, int n) {
        float I = getPathLength(src, srcCount) / (n - 1);
        if (I <= 0.0f) {
            for (int i = 0; i < n; ++i) dst[i] = src[0];
            return;
        }

        dst[0] = src[0];
        int dstIndex = 1;
        float D = 0.0f;

        GesturePoint current = src[0];
        for (int i = 1; i < srcCount && dstIndex < n; ++i) {
            float d = distance(current, src[i]);
            if ((D + d) >= I) {
                float qx = current.x + ((I - D) / d) * (src[i].x - current.x);
                float qy = current.y + ((I - D) / d) * (src[i].y - current.y);
                GesturePoint q = { qx, qy };
                dst[dstIndex++] = q;
                current = q;
                D = 0.0f;
                --i; // Reavalia a partir do novo ponto intermediário
            } else {
                D += d;
                current = src[i];
            }
        }

        while (dstIndex < n) {
            dst[dstIndex++] = src[srcCount - 1];
        }
    }

    static void scaleTo(GesturePoint* pts, int n, float size) {
        float minX = pts[0].x, maxX = pts[0].x;
        float minY = pts[0].y, maxY = pts[0].y;
        for (int i = 1; i < n; ++i) {
            if (pts[i].x < minX) minX = pts[i].x;
            if (pts[i].x > maxX) maxX = pts[i].x;
            if (pts[i].y < minY) minY = pts[i].y;
            if (pts[i].y > maxY) maxY = pts[i].y;
        }
        float w = maxX - minX;
        float h = maxY - minY;
        if (w < 0.001f) w = 1.0f;
        if (h < 0.001f) h = 1.0f;
        float maxDim = (w > h) ? w : h;
        for (int i = 0; i < n; ++i) {
            pts[i].x *= (size / maxDim);
            pts[i].y *= (size / maxDim);
        }
    }

    static void translateToOrigin(GesturePoint* pts, int n) {
        float sumX = 0.0f, sumY = 0.0f;
        for (int i = 0; i < n; ++i) {
            sumX += pts[i].x;
            sumY += pts[i].y;
        }
        float cx = sumX / n;
        float cy = sumY / n;
        for (int i = 0; i < n; ++i) {
            pts[i].x -= cx;
            pts[i].y -= cy;
        }
    }

    static float matchTemplate(const GesturePoint* candidate, const GesturePoint* tmpl) {
        float totalDist = 0.0f;
        for (int i = 0; i < N_POINTS; ++i) {
            totalDist += distance(candidate[i], tmpl[i]);
        }
        return totalDist / N_POINTS;
    }

    // ============================================================
    // TEMPLATES CANÔNICOS PRE-CALCULADOS (32 pontos cada)
    // ============================================================
    static const GesturePoint* getCircleTemplate(bool cw) {
        static GesturePoint circleCW[N_POINTS];
        static GesturePoint circleCCW[N_POINTS];
        static bool init = false;
        if (!init) {
            for (int i = 0; i < N_POINTS; ++i) {
                float thetaCW = (2.0f * M_PI * i) / (N_POINTS - 1);
                float thetaCCW = -thetaCW;
                circleCW[i].x = cosf(thetaCW) * 50.0f;
                circleCW[i].y = sinf(thetaCW) * 50.0f;
                circleCCW[i].x = cosf(thetaCCW) * 50.0f;
                circleCCW[i].y = sinf(thetaCCW) * 50.0f;
            }
            init = true;
        }
        return cw ? circleCW : circleCCW;
    }

    static const GesturePoint* getVTemplate() {
        static GesturePoint vTmpl[N_POINTS];
        static bool init = false;
        if (!init) {
            int half = N_POINTS / 2;
            for (int i = 0; i < half; ++i) {
                vTmpl[i].x = (float)i * (50.0f / half) - 25.0f;
                vTmpl[i].y = (float)i * (100.0f / half) - 50.0f;
            }
            for (int i = half; i < N_POINTS; ++i) {
                vTmpl[i].x = 25.0f + (float)(i - half) * (50.0f / (N_POINTS - half)) - 25.0f;
                vTmpl[i].y = 50.0f - (float)(i - half) * (100.0f / (N_POINTS - half));
            }
            init = true;
        }
        return vTmpl;
    }

    static const GesturePoint* getZTemplate() {
        static GesturePoint zTmpl[N_POINTS];
        static bool init = false;
        if (!init) {
            int s1 = N_POINTS / 3;
            int s2 = 2 * N_POINTS / 3;
            for (int i = 0; i < s1; ++i) {
                zTmpl[i].x = ((float)i * (100.0f / s1)) - 50.0f;
                zTmpl[i].y = -50.0f;
            }
            for (int i = s1; i < s2; ++i) {
                zTmpl[i].x = (50.0f - (float)(i - s1) * (100.0f / (s2 - s1)));
                zTmpl[i].y = (-50.0f + (float)(i - s1) * (100.0f / (s2 - s1)));
            }
            for (int i = s2; i < N_POINTS; ++i) {
                zTmpl[i].x = (-50.0f + (float)(i - s2) * (100.0f / (N_POINTS - s2)));
                zTmpl[i].y = 50.0f;
            }
            init = true;
        }
        return zTmpl;
    }

    static const GesturePoint* getTriangleTemplate() {
        static GesturePoint triTmpl[N_POINTS];
        static bool init = false;
        if (!init) {
            int s1 = N_POINTS / 3;
            int s2 = 2 * N_POINTS / 3;
            for (int i = 0; i < s1; ++i) { // Sobe para o pico
                triTmpl[i].x = ((float)i * (50.0f / s1)) - 25.0f;
                triTmpl[i].y = 50.0f - ((float)i * (100.0f / s1));
            }
            for (int i = s1; i < s2; ++i) { // Desce para a direita
                triTmpl[i].x = 25.0f + ((float)(i - s1) * (50.0f / (s2 - s1))) - 25.0f;
                triTmpl[i].y = -50.0f + ((float)(i - s1) * (100.0f / (s2 - s1)));
            }
            for (int i = s2; i < N_POINTS; ++i) { // Base para a esquerda
                triTmpl[i].x = 50.0f - ((float)(i - s2) * (100.0f / (N_POINTS - s2)));
                triTmpl[i].y = 50.0f;
            }
            init = true;
        }
        return triTmpl;
    }

    static void fillMetadata(GestureResult& res) {
        switch (res.type) {
            case GESTURE_CIRCLE:
                res.name = "CIRCULO";
                res.action = "TV POWER DISPARADO";
                res.symbol = "O";
                res.color = 0xF68D; // UI_YELLOW
                break;
            case GESTURE_CHECK_V:
                res.name = "LETRA V";
                res.action = "TV MUTE / DESMUDO";
                res.symbol = "V";
                res.color = 0x5F37; // UI_GREEN
                break;
            case GESTURE_ZIGZAG:
                res.name = "LETRA Z (RAIO)";
                res.action = "AR-CONDICIONADO POWER";
                res.symbol = "Z";
                res.color = 0x05BF; // Ciano vibrante
                break;
            case GESTURE_TRIANGLE:
                res.name = "TRIANGULO";
                res.action = "MODO TURBO AR";
                res.symbol = "/\\";
                res.color = 0xFD20; // UI_ORANGE
                break;
            default:
                break;
        }
    }
};
