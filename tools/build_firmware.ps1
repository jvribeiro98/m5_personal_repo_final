param(
  [string]$ArduinoCli = 'arduino-cli',
  [string]$BuildPath = '',
  [string]$StagePath = '',
  [switch]$Upload,
  [string]$Port = 'COM3'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $BuildPath) { $BuildPath = Join-Path $projectRoot '.build/output' }
if (-not $StagePath) { $StagePath = Join-Path $projectRoot '.build/m5_personal' }
$BuildPath = [IO.Path]::GetFullPath($BuildPath)
$StagePath = [IO.Path]::GetFullPath($StagePath)
if ((Split-Path $StagePath -Leaf) -ne 'm5_personal') { throw 'StagePath deve terminar em m5_personal.' }
if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
  $ArduinoCli = Join-Path $env:LOCALAPPDATA 'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'
}
if (-not (Test-Path -LiteralPath $ArduinoCli) -and -not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) { throw 'Arduino CLI nao encontrado.' }
New-Item -ItemType Directory -Force $BuildPath,$StagePath | Out-Null
# Copy only canonical source files. A failed build must never be marked as current.
Get-ChildItem (Join-Path $projectRoot 'firmware') -File | Where-Object { $_.Extension -in '.ino','.h','.cpp' } | Copy-Item -Destination $StagePath -Force
$buildManifest = Join-Path $BuildPath 'build-manifest.json'
if (Test-Path -LiteralPath $buildManifest) { Remove-Item -LiteralPath $buildManifest }
$fqbn = 'esp32:esp32:m5stack_stickc_plus2:UploadSpeed=1500000,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,DebugLevel=none,PSRAM=enabled,LoopCore=1,EventsCore=1,EraseFlash=none'
& $ArduinoCli compile --fqbn $fqbn --build-path $BuildPath $StagePath
if ($LASTEXITCODE -ne 0) { throw 'Compilacao falhou; nenhum binario foi aprovado para gravacao.' }
$sources = @{}
Get-ChildItem (Join-Path $projectRoot 'firmware') -File | Where-Object { $_.Extension -in '.ino','.h','.cpp' } | ForEach-Object {
  $sources[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
}
$binary = Join-Path $BuildPath 'm5_personal.ino.bin'
@{ builtAt=[DateTime]::UtcNow.ToString('o'); sourceRoot=$projectRoot; fqbn=$fqbn; sources=$sources; binarySha256=(Get-FileHash -LiteralPath $binary).Hash } |
  ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $buildManifest -Encoding utf8
if ($Upload) {
  & $ArduinoCli upload --fqbn ($fqbn.Replace('UploadSpeed=1500000','UploadSpeed=500000')) --port $Port --input-dir $BuildPath $StagePath
  if ($LASTEXITCODE -ne 0) { throw 'A gravacao nao foi concluida.' }
}
