"""Run production firmware functions against deterministic hardware doubles.

Only the hardware boundary is simulated. Functions are extracted unchanged
from the sketch so these tests can run before and after a firmware fix.
"""
from pathlib import Path
import os
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
    'toggleAcPower', 'drawWifiSavedDetail',
    'executeAcAction', 'scanNetworksNow', 'sortScannedNetworksByRssi', 'tryAutoConnectStrongest',
    'wifiKeyboard',
    'deleteSavedNetwork',
]

def function(name):
    import re
    match = re.search(r'^(?:void|bool|String) ' + name + r'\([^;]*?\) \{', source, re.M)
    if not match:
        raise RuntimeError('Missing production function: ' + name)
    end = source.index('\n}', match.end()) + 2
    return source[match.start():end]

types = source[source.index('enum class Screen'):source.index('// GLOBAIS')]
globals_ = source[source.index('Preferences prefs;'):source.index('// PROTOTIPOS EXPLICITOS')]
optional = ['parseIndexArg', 'acStateJson', 'handleWebApiAcState', 'wifiOperationBusy', 'acMinTemp']
optional += ['requestWifiScan', 'processWifiScan']
for name in optional:
    if ('bool ' + name + '(' in source or 'String ' + name + '(' in source or 'void ' + name + '(' in source):
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
sys.exit(subprocess.run([str(executable)]).returncode)
