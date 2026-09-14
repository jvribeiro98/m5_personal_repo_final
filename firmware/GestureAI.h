#ifndef GESTURE_AI_H
#define GESTURE_AI_H

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>
#endif
#include <cmath>

// ============================================================
// GESTURE AI - Motor Inercial 3D de Reconhecimento no Ar
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
    GESTURE_ROLL_CW,
    GESTURE_ROLL_CCW,
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
    static constexpr int MAX_RAW_POINTS = 160;

    GestureRecognizer() : _rawCount(0), _recording(false), _lastSampleMs(0),
                          _accumX(0.0f), _accumY(0.0f), _accumRoll(0.0f),
                          _maxThrustG(0.0f), _biasGx(0.0f), _biasGz(0.0f) {}

    void startRecording() {
        _rawCount = 0;
        _accumX = 0.0f;
        _accumY = 0.0f;
        _accumRoll = 0.0f;
        _maxThrustG = 0.0f;
        _recording = true;
        _lastSampleMs = 0;
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

    float getAccumRoll() const {
        return _accumRoll;
    }

    // Amostragem contínua enquanto o botão está pressionado (~50-80 Hz)
    void sample(float gx, float gy, float gz, float ax, float ay, float az, uint32_t nowMs) {
        if (!_recording) return;

        if (_lastSampleMs == 0) {
            _lastSampleMs = nowMs;
            _biasGx = gx;
            _biasGz = gz;
            _rawPoints[0].x = 0.0f;
            _rawPoints[0].y = 0.0f;
            _rawCount = 1;
            return;
        }

        uint32_t diff = nowMs - _lastSampleMs;
        if (diff < 12) return; // Limita taxa maxima de amostragem (~70Hz)
        float dt = diff / 1000.0f;
        if (dt > 0.08f) dt = 0.02f;
        _lastSampleMs = nowMs;

        // Suave filtro para eliminar bias estático inicial
        float egx = gx - _biasGx * 0.4f;
        float egz = gz - _biasGz * 0.4f;

        // Deslocamento angular acumulado (em graus):
        // Pitch (gx): mover para cima/baixo
        // Yaw (gz): mover para esquerda/direita
        // Roll (gy): torcer o pulso como chave/dial
        float vx = -egz * dt;
        float vy = -egx * dt;
        _accumRoll += gy * dt;

        _accumX += vx;
        _accumY += vy;

        // Aceleração resultante de impacto frontal (estocada)
        float thrust = sqrtf(ax * ax + ay * ay + az * az);
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
        res.color = 0x8CD1; // Slate

        if (_rawCount < 8) {
            res.action = "Movimento muito curto";
            return res;
        }

        // 1. Giro de Pulso (Twist / Roll)
        // Se girou o punho sem desenhar muito no ar
        float totalDisp = distance(_rawPoints[0], _rawPoints[_rawCount - 1]);
        if (fabsf(_accumRoll) > 40.0f && totalDisp < 30.0f) {
            if (_accumRoll > 0.0f) {
                res.type = GESTURE_ROLL_CW;
                res.confidence = 94.0f;
                res.name = "GIRO HORARIO";
                res.action = "TV VOLUME +";
                res.symbol = "(+)";
                res.color = 0x27E8; // Cyber mint
                return res;
            } else {
                res.type = GESTURE_ROLL_CCW;
                res.confidence = 94.0f;
                res.name = "GIRO ANTI-HORARIO";
                res.action = "TV VOLUME -";
                res.symbol = "(-)";
                res.color = 0xF9C7; // Coral
                return res;
            }
        }

        // 2. Estocada frontal brusca (Thrust / Jab)
        float pathLen = getPathLength(_rawPoints, _rawCount);
        if (_maxThrustG > 2.4f && pathLen < 25.0f) {
            res.type = GESTURE_THRUST;
            res.confidence = 92.0f;
            res.name = "ESTOCADA";
            res.action = "TV OK / SELECIONAR";
            res.symbol = "[OK]";
            res.color = 0xFFFF;
            return res;
        }

        if (pathLen < 12.0f) {
            res.action = "Movimento muito pequeno";
            return res;
        }

        float dx = _rawPoints[_rawCount - 1].x - _rawPoints[0].x;
        float dy = _rawPoints[_rawCount - 1].y - _rawPoints[0].y;
        float straightDist = sqrtf(dx * dx + dy * dy);
        float closureRatio = straightDist / pathLen;

        // 3. Swipes Rápidos (Linhas retas direcionais)
        if (closureRatio > 0.72f && pathLen > 16.0f) {
            if (fabsf(dx) > fabsf(dy) * 1.35f) {
                if (dx > 0.0f) {
                    res.type = GESTURE_SWIPE_RIGHT;
                    res.confidence = 95.0f;
                    res.name = "SWIPE DIREITA";
                    res.action = "CANAL +";
                    res.symbol = ">>";
                    res.color = 0x067F; // Electric cyan
                    return res;
                } else {
                    res.type = GESTURE_SWIPE_LEFT;
                    res.confidence = 95.0f;
                    res.name = "SWIPE ESQUERDA";
                    res.action = "CANAL -";
                    res.symbol = "<<";
                    res.color = 0x067F;
                    return res;
                }
            } else if (fabsf(dy) > fabsf(dx) * 1.35f) {
                if (dy < 0.0f) { // dy negativo = movimento para cima
                    res.type = GESTURE_SWIPE_UP;
                    res.confidence = 95.0f;
                    res.name = "SWIPE CIMA";
                    res.action = "VOLUME +";
                    res.symbol = "^";
                    res.color = 0x27E8;
                    return res;
                } else {
                    res.type = GESTURE_SWIPE_DOWN;
                    res.confidence = 95.0f;
                    res.name = "SWIPE BAIXO";
                    res.action = "VOLUME -";
                    res.symbol = "v";
                    res.color = 0xF9C7;
                    return res;
                }
            }
        }

        // 4. Métricas Geométricas Invariantes (Bounding Box e Winding Angle)
        float minX = _rawPoints[0].x, maxX = _rawPoints[0].x;
        float minY = _rawPoints[0].y, maxY = _rawPoints[0].y;
        for (int i = 1; i < _rawCount; ++i) {
            if (_rawPoints[i].x < minX) minX = _rawPoints[i].x;
            if (_rawPoints[i].x > maxX) maxX = _rawPoints[i].x;
            if (_rawPoints[i].y < minY) minY = _rawPoints[i].y;
            if (_rawPoints[i].y > maxY) maxY = _rawPoints[i].y;
        }
        float bbW = maxX - minX;
        float bbH = maxY - minY;
        if (bbW < 0.1f) bbW = 0.1f;
        if (bbH < 0.1f) bbH = 0.1f;
        float aspect = bbW / bbH;

        // Soma acumulada dos ângulos tangenciais (Winding Turns)
        float headingTurns = 0.0f;
        float prevHeading = 0.0f;
        bool hasPrevHeading = false;
        int step = (_rawCount > 40) ? 2 : 1;

        for (int i = step; i < _rawCount; i += step) {
            float segDx = _rawPoints[i].x - _rawPoints[i - step].x;
            float segDy = _rawPoints[i].y - _rawPoints[i - step].y;
            float segDist = sqrtf(segDx * segDx + segDy * segDy);
            if (segDist > 0.4f) {
                float hAng = atan2f(segDy, segDx);
                if (hasPrevHeading) {
                    float dAng = hAng - prevHeading;
                    while (dAng > (float)M_PI) dAng -= 2.0f * (float)M_PI;
                    while (dAng < -(float)M_PI) dAng += 2.0f * (float)M_PI;
                    headingTurns += dAng;
                }
                prevHeading = hAng;
                hasPrevHeading = true;
            }
        }
        float absTurnDeg = fabsf(headingTurns) * 180.0f / (float)M_PI;

        // 5. Reconhecimento de CÍRCULO (Invariante a ponto de partida e sentido)
        // Traço fechado, proporção aproximada de 1:1, giro de 360° (+- 80°)
        if (closureRatio < 0.48f && aspect > 0.40f && aspect < 2.4f && absTurnDeg > 250.0f && absTurnDeg < 540.0f) {
            res.type = GESTURE_CIRCLE;
            res.confidence = 94.0f;
            res.name = "CIRCULO";
            res.action = "TV LIGAR / DESLIGAR";
            res.symbol = "( O )";
            res.color = 0xFDE0; // Ouro Cyber
            return res;
        }

        // 6. Reconhecimento de LETRA V (Checkmark)
        // Começa descendo (dy > 0), atinge vértice inferior no terço central e sobe (dy < 0)
        int n = _rawCount;
        float firstHalfDy = _rawPoints[n / 3].y - _rawPoints[0].y;
        float lastHalfDy = _rawPoints[n - 1].y - _rawPoints[2 * n / 3].y;
        int minYIdx = 0;
        for (int i = 1; i < n; ++i) {
            if (_rawPoints[i].y > _rawPoints[minYIdx].y) minYIdx = i; // y cresce para baixo
        }
        if (firstHalfDy > 4.0f && lastHalfDy < -4.0f && minYIdx >= n / 4 && minYIdx <= (3 * n) / 4) {
            res.type = GESTURE_CHECK_V;
            res.confidence = 91.0f;
            res.name = "LETRA V";
            res.action = "TV MUTE / DESMUDO";
            res.symbol = "[ V ]";
            res.color = 0x27E8; // Cyber mint
            return res;
        }

        // 7. Reconhecimento de LETRA Z (Zigzag / Raio)
        // 3 segmentos: direita -> descida inclinada esquerda -> direita
        float s1_dx = _rawPoints[n / 3].x - _rawPoints[0].x;
        float s2_dx = _rawPoints[2 * n / 3].x - _rawPoints[n / 3].x;
        float s3_dx = _rawPoints[n - 1].x - _rawPoints[2 * n / 3].x;
        if (s1_dx > 4.0f && s2_dx < -4.0f && s3_dx > 4.0f && dy > 6.0f) {
            res.type = GESTURE_ZIGZAG;
            res.confidence = 92.0f;
            res.name = "LETRA Z (RAIO)";
            res.action = "AR LIGAR / DESLIGAR";
            res.symbol = "[ Z ]";
            res.color = 0x067F; // Electric cyan
            return res;
        }

        // 8. Reconhecimento de TRIÂNGULO
        if (closureRatio < 0.48f && absTurnDeg > 220.0f && absTurnDeg < 480.0f) {
            res.type = GESTURE_TRIANGLE;
            res.confidence = 86.0f;
            res.name = "TRIANGULO";
            res.action = "AR MODO TURBO";
            res.symbol = "[ /\\ ]";
            res.color = 0xFD20; // Laranja neon
            return res;
        }

        // Não reconhecido
        res.type = GESTURE_NONE;
        res.confidence = 25.0f;
        res.name = "INCERTO";
        res.action = "Faça Circulo, V, Z ou Swipe";
        res.symbol = "[ ? ]";
        res.color = 0x8CD1;
        return res;
    }

private:
    GesturePoint _rawPoints[MAX_RAW_POINTS];
    int _rawCount;
    bool _recording;
    uint32_t _lastSampleMs;
    float _accumX;
    float _accumY;
    float _accumRoll;
    float _maxThrustG;
    float _biasGx;
    float _biasGz;

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
};

#endif // GESTURE_AI_H
