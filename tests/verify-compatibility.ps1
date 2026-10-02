$ErrorActionPreference='Stop'
$source=Get-Content -LiteralPath 'build\compatibility\exports.txt' -Raw
$exports=[regex]::Matches($source,'(?m)^\s+(\d+)\s+[0-9A-F]+\s+[0-9A-F]+\s+(\w+)\s*$')
$expected=@('DirectInput8Create','DllCanUnloadNow','DllGetClassObject','DllRegisterServer','DllUnregisterServer','GetdfDIJoystick')
if($exports.Count -ne 6) {throw 'Unexpected compatibility export count'}
for($i=0;$i -lt 6;$i++) {
    if([int]$exports[$i].Groups[1].Value -ne ($i+1) -or $exports[$i].Groups[2].Value -ne $expected[$i]) {throw 'Compatibility export mismatch'}
}
Write-Host 'PASS: six public DirectInput exports and ordinals.'
