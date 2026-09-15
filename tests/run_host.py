"""Run production firmware functions against deterministic hardware doubles.

Only the hardware boundary is simulated. Functions are extracted unchanged
from the sketch so these tests can run before and after a firmware fix.
"""
from pathlib import Path
import os
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = Path(os.environ.get('M5_TEST_BUILD', root / '.test-build')).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (root / 'firmware/m5_personal.ino').read_text(encoding='utf-8')
names = [
    'jsonEscape', 'sendJson',
    'beginWifiConnection', 'processWifiConnection', 'handleWebApiStatus',
    'handleWebApiTv', 'handleWebApiAc',
    'handleWebApiSaveNetwork', 'handleWebApiDeleteSaved', 'handleWebApiConnect',
    'toggleAcPower',
    'executeAcAction', 'scanNetworksNow', 'sortScannedNetworksByRssi', 'tryAutoConnectStrongest',
    'wifiKeyboard',
    'deleteSavedNetwork',
]

def function(name):
    match = re.search(r'^(?:void|bool|String|uint8_t) ' + name + r'\([^;]*?\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError('Missing production function: ' + name)
    # Count real braces, ignoring braces inside C++ strings and comments.
    depth = 1
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    for token in tokens.finditer(source, match.end()):
        if token.group() == '{': depth += 1
        elif token.group() == '}': depth -= 1
        if depth == 0:
            return source[match.start():token.end()]
    raise RuntimeError('Unterminated production function: ' + name)

types = source[source.index('enum class Screen'):source.index('// GLOBAIS')]
# Only Wi-Fi/IR state is used by this harness. Including the entire globals
# section also pulls in BLE, audio transports and inline hardware functions.
globals_ = source[source.index('Preferences prefs;'):source.index('constexpr uint32_t TEAM_A_HOLD_MS')]
globals_ += source[source.index('TvDevice televisions[]'):source.index('// PROTOTIPOS EXPLICITOS')]
globals_ += re.search(r'^uint32_t lastWeatherAttemptAt = .*?;', source, re.M).group(0)
optional = ['parseIndexArg', 'acStateJson', 'handleWebApiAcState', 'wifiOperationBusy', 'acMinTemp']
optional += ['requestWifiScan', 'processWifiScan']
for name in optional:
    if re.search(r'^(?:void|bool|String|uint8_t) ' + name + r'\([^;]*?\)\s*\{', source, re.M):
        names.insert(0, name)
constants = source[source.index('constexpr uint8_t IR_PIN'):source.index('// TIPOS')]
keys = source[source.index('const char BRUCE_KEYS'):source.index('String wifiKeyboard(const String& title, const String& initial, bool masked, bool& cancelled) {')]
unit = '#include "host_stubs.h"\n' + constants + types + globals_ + keys
unit += '\n#include "host_support.h"\n'
if 'void processWifiScan() {' in source:
    unit += '#define HAS_ASYNC_SCAN 1\n'
functions = [function(name) for name in names]
unit += '\n'.join(code[:code.index('{')].strip() + ';' for code in functions) + '\n'
unit += '\n'.join(code.replace('const String& extra = ""', 'const String& extra') for code in functions)
unit += '\n#include "host_cases.h"\n'
(out / 'test.cpp').write_text(unit, encoding='utf-8')
if os.name == 'nt':
    vcvars = os.environ.get('M5_VCVARS', r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat')
    script = out / 'compile.cmd'
    script.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /std:c++17 /EHsc /utf-8 /I"{root / "tests"}" "{out / "test.cpp"}" /Fe:"{out / "test.exe"}" /Fo:"{out / "test.obj"}"\n', encoding='utf-8')
    subprocess.run(['cmd', '/c', str(script)], check=True, cwd=out)
    executable = out / 'test.exe'
else:
    executable = out / 'test'
    subprocess.run(['g++', '-std=c++17', '-I' + str(root / 'tests'), str(out / 'test.cpp'), '-o', str(executable)], check=True)
sys.exit(subprocess.run([str(executable)], timeout=30).returncode)
