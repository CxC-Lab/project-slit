# Manual Windows GUI verification; SendKeys deliberately stays outside CTest.
param([string]$BeforeDiagnostic,[string]$Tileset,[string]$CaptureOutput)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
$bin=Join-Path $root 'build/Release'
$work=Join-Path $root ('build/run_entry_checks/'+[Guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory $work
$captureFolder=Join-Path $root 'build/preview'
$region=Get-Content (Join-Path $root 'assets/regions/avatar_lake.json') -Raw | ConvertFrom-Json
$default=Get-Content (Join-Path $root ('assets/tilesets/'+$region.tileset+'.json')) -Raw | ConvertFrom-Json
$tilesetDir=Join-Path $root 'assets/tilesets'
Add-Type -TypeDefinition @'
public static class RunEntryHash {
    public static string Fnv1a64(string path) {
        ulong value = 14695981039346656037UL;
        foreach (byte b in System.IO.File.ReadAllBytes(path))
            value = unchecked((value ^ b) * 1099511628211UL);
        return value.ToString("x16");
    }
}
'@
if($Tileset) {
    # Generic explicit-tileset capture; no candidate-specific launcher or key sequence.
    if($Tileset -notmatch '^[a-z0-9_]+$') { throw 'Invalid tileset name' }
    $set=Get-Content (Join-Path $tilesetDir ($Tileset+'.json')) -Raw | ConvertFrom-Json
    if(-not $CaptureOutput) { $CaptureOutput=$work }
    $null=New-Item -ItemType Directory -Force $CaptureOutput
    $outputJson=Join-Path $CaptureOutput 'game_capture.json'
    $outputPng=Join-Path $CaptureOutput 'game_capture.png'
    if((Test-Path $outputJson) -or (Test-Path $outputPng)) { throw 'Capture destination already exists' }
    $before=@(Get-ChildItem $captureFolder -Filter 'captured_*.json' | ForEach-Object Name)
    $game=Start-Process "$bin/project_slit.exe" -ArgumentList @('avatar_lake','--tileset',$Tileset) -WorkingDirectory $root -PassThru
    $null=$game.Handle
    try {
        Start-Sleep -Seconds 3
        $game.Refresh()
        if($game.HasExited) { throw 'Game crashed before capture' }
        $shell=New-Object -ComObject WScript.Shell
        if(-not $shell.AppActivate($game.Id)) { throw 'Game focus failed' }
        Start-Sleep -Milliseconds 500
        $shell.SendKeys('{F12}')
        $new=$null
        for($i=0;$i -lt 50;$i++) {
            Start-Sleep -Milliseconds 100
            $new=Get-ChildItem $captureFolder -Filter 'captured_*.json' | Where-Object { $before -notcontains $_.Name -and $_.Length -gt 0 } | Select-Object -First 1
            if($new) { break }
        }
        if(-not $new) { throw 'F12 capture missing' }
        $data=Get-Content $new.FullName -Raw | ConvertFrom-Json
        if($data.tileset -ne $Tileset -or $data.tilesetMode -ne 'override') { throw 'Capture tileset mismatch' }
        foreach($entry in $set.sampling.PSObject.Properties) {
            $image=[IO.Path]::GetFullPath((Join-Path $tilesetDir $entry.Value.image))
            $identified=@($data.samplingImages | Where-Object role -eq $entry.Name)
            if($identified.Count -ne 1 -or $identified[0].mode -ne $entry.Value.mode -or
               [IO.Path]::GetFullPath($identified[0].image) -ne $image -or
               $identified[0].hashAlgorithm -ne 'fnv1a64' -or $identified[0].hash -ne [RunEntryHash]::Fnv1a64($image)) { throw 'Sampling image identity mismatch' }
        }
        Copy-Item $new.FullName $outputJson
        Copy-Item ([IO.Path]::ChangeExtension($new.FullName,'.png')) $outputPng
        if(-not $game.CloseMainWindow() -or -not $game.WaitForExit(5000) -or $game.ExitCode -ne 0) { throw 'Game did not close normally' }
        Write-Output "CAPTURE PASS: $outputJson; aliveAfter3Seconds=True exitCode=$($game.ExitCode)"
    } finally {
        $game.Refresh()
        if(-not $game.HasExited) { $game.Kill();$game.WaitForExit() }
        $game.Dispose()
    }
    exit 0
}
if(-not $BeforeDiagnostic) { throw 'BeforeDiagnostic is required for entry regression checks' }
$runtime=$null
$expectedHash=''
if($default.sampling.inner.image) {
$runtime=[IO.Path]::GetFullPath((Join-Path $tilesetDir $default.sampling.inner.image))
$record=Get-Content ([IO.Path]::ChangeExtension($runtime,'.provenance.json')) -Raw | ConvertFrom-Json
$runtimeBefore=(Get-FileHash $runtime -Algorithm SHA256).Hash
$recordPath=[IO.Path]::ChangeExtension($runtime,'.provenance.json')
$recordBefore=(Get-FileHash $recordPath -Algorithm SHA256).Hash
$prefix=[IO.Path]::GetFullPath($root).TrimEnd('\')+'\'
if(-not $runtime.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Runtime image outside repository' }
$relative=$runtime.Substring($prefix.Length)
if($runtimeBefore -ne $record.runtime_sha256 -or $relative -match '^build[\\/]') { throw 'Invalid approved runtime image' }
$expectedHash=[RunEntryHash]::Fnv1a64($runtime)
}
$trial=$null
$plan=Get-Content (Join-Path $PSScriptRoot 'inner_macro_capture.json') -Raw | ConvertFrom-Json
function SamplingSignature($set) {
    @($set.sampling.PSObject.Properties | Sort-Object Name | ForEach-Object {
        [ordered]@{role=$_.Name;mode=$_.Value.mode;
            image=[IO.Path]::GetFullPath((Join-Path $tilesetDir $_.Value.image)).ToLowerInvariant();
            flip=$(if($_.Value.flip){$_.Value.flip}else{''})}
    }) | ConvertTo-Json -Depth 4 -Compress
}
$defaultSampling=SamplingSignature $default
foreach($entry in $plan.trials) {
    # An empty plan override means the region default; compare it via explicit override.
    $name=if($entry.tileset){$entry.tileset}else{$region.tileset}
    $data=Get-Content (Join-Path $tilesetDir ($name+'.json')) -Raw | ConvertFrom-Json
    if((SamplingSignature $data) -eq $defaultSampling) {
        $trial=$name;break
    }
}
if(-not $trial) { throw 'No comparison trial with identical sampling in capture plan' }
function Preview([string]$name,[string]$tileset,[string]$point='877,355') {
    $args=@('avatar_lake',$point,'--out',(Join-Path $work $name))
    if($tileset) { $args+=@('--tileset',$tileset) }
    & "$bin/preview.exe" @args
    if($LASTEXITCODE -ne 0) { throw "Preview failed: $name" }
}
Preview 'approved' ''
Preview 'debug' 'avatar_lake_debug'
Preview 'trial' $trial
foreach($pair in @(@("$work/approved/preview_1.png","$work/trial/preview_1.png"),@("$work/debug/preview_1.png",$BeforeDiagnostic))) {
    & "$bin/tile_pipeline.exe" --compare-images $pair[0] $pair[1]
    if($LASTEXITCODE -ne 0) { throw 'Same-camera RGBA comparison failed' }
}
$results=@()
function Close-TestProcess($process) {
    if(-not $process) { return }
    try {
        $process.Refresh()
        if(-not $process.HasExited) { $process.Kill();$process.WaitForExit() }
    } catch {
        # A short-lived cmd.exe can exit between HasExited and Kill.
        if(Get-Process -Id $process.Id -ErrorAction SilentlyContinue) { throw }
    } finally { $process.Dispose() }
}
foreach($cwd in @($root,[IO.Path]::GetTempPath())) {
    foreach($debug in @($false,$true)) {
        $bat=Join-Path $PSScriptRoot $(if($debug){'run_avatar_lake_debug.bat'}else{'run_avatar_lake.bat'})
        $before=@(Get-ChildItem $captureFolder -Filter 'captured_*.json' | ForEach-Object Name)
        Write-Output "Starting BAT=$bat WorkingDirectory=$cwd"
        $launcher=Start-Process $env:ComSpec -ArgumentList @('/d','/c','call',('"'+$bat+'"')) -WorkingDirectory $cwd -PassThru -RedirectStandardOutput "$work/launch_$($results.Count).out" -RedirectStandardError "$work/launch_$($results.Count).err"
        $null=$launcher.Handle
        $game=$null
        try {
            for($i=0;$i -lt 40;$i++) {
                $child=Get-CimInstance Win32_Process -Filter "ParentProcessId=$($launcher.Id)" | Where-Object Name -eq 'project_slit.exe' | Select-Object -First 1
                if($child) { $game=Get-Process -Id $child.ProcessId; $null=$game.Handle;break }
                if($launcher.HasExited) { throw "BAT exited before game launch: $($launcher.ExitCode)" }
                Start-Sleep -Milliseconds 250
            }
            if(-not $game) { throw 'BAT game process not found' }
            Start-Sleep -Seconds 3
            $game.Refresh()
            if($game.HasExited) { throw 'Game crashed before 3 seconds' }
            $shell=New-Object -ComObject WScript.Shell
            if(-not $shell.AppActivate($game.Id)) { throw 'Game focus failed' }
            Start-Sleep -Milliseconds 500
            $shell.SendKeys('{F12}')
            $new=$null
            for($i=0;$i -lt 50;$i++) {
                Start-Sleep -Milliseconds 100
                $new=Get-ChildItem $captureFolder -Filter 'captured_*.json' | Where-Object { $before -notcontains $_.Name -and $_.Length -gt 0 } | Select-Object -First 1
                if($new) { break }
            }
            if(-not $new) { throw 'F12 capture missing' }
            $data=Get-Content $new.FullName -Raw | ConvertFrom-Json
            if($debug) {
                if($data.tileset -ne 'avatar_lake_debug' -or $data.tilesetMode -ne 'override' -or $data.innerMacroImage) { throw 'Debug identity mismatch' }
            } else {
                if($data.tileset -ne $region.tileset -or $data.tilesetMode -ne 'region_default') { throw 'Approved default identity mismatch' }
                if($runtime) {
                    if(-not $data.innerMacroImage -or [IO.Path]::GetFullPath($data.innerMacroImage) -ne $runtime) { throw 'Approved image mismatch' }
                } elseif($data.innerMacroImage) { throw 'Unexpected inner macro image' }
                if($data.innerMacroHash -ne $expectedHash) { throw 'Approved capture hash mismatch' }
            }
            if(-not $game.CloseMainWindow() -or -not $game.WaitForExit(5000) -or $game.ExitCode -ne 0) { throw 'Game did not close normally' }
            if(-not $launcher.WaitForExit(5000) -or $launcher.ExitCode -ne 0) { throw 'BAT did not exit normally' }
            $coordinate=$data.cameraCenter[0].ToString([Globalization.CultureInfo]::InvariantCulture)+','+$data.cameraCenter[1].ToString([Globalization.CultureInfo]::InvariantCulture)
            $match='match_'+$results.Count
            Preview $match $(if($debug){'avatar_lake_debug'}else{''}) $coordinate
            & "$bin/terrain_tiles_tests.exe" --compare ([IO.Path]::ChangeExtension($new.FullName,'.png')) "$work/$match/preview_1.png"
            if($LASTEXITCODE -ne 0) { throw 'F12/preview terrain comparison failed' }
            $results+=[ordered]@{bat=$bat;workingDirectory=$cwd;aliveAfter3Seconds=$true;gameExit=$game.ExitCode;batExit=$launcher.ExitCode;capture=$new.FullName;metadata=$data}
            [IO.File]::WriteAllText("$work/report.json",($results | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
        } finally {
            Close-TestProcess $game
            Close-TestProcess $launcher
        }
    }
}
# Existing EXE entry points remain available independently of the BAT aliases.
foreach($mode in @('practice','trial')) {
    if($mode -eq 'practice') { $p=Start-Process "$bin/project_slit.exe" -WorkingDirectory $root -PassThru }
    else { $p=Start-Process "$bin/project_slit.exe" -ArgumentList @('avatar_lake','--tileset',$trial) -WorkingDirectory $root -PassThru }
    $null=$p.Handle
    try {
        Start-Sleep -Seconds 3
        $p.Refresh()
        if($p.HasExited -or -not $p.CloseMainWindow() -or -not $p.WaitForExit(5000) -or $p.ExitCode -ne 0) { throw "Existing entry failed: $mode" }
        $results+=[ordered]@{entry=$mode;aliveAfter3Seconds=$true;gameExit=$p.ExitCode}
    } finally { Close-TestProcess $p }
}
if($runtime -and ((Get-FileHash $runtime -Algorithm SHA256).Hash -ne $runtimeBefore -or (Get-FileHash $recordPath -Algorithm SHA256).Hash -ne $recordBefore)) { throw 'Runtime/provenance changed' }
[IO.File]::WriteAllText("$work/report.json",($results | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
Write-Output "RUN ENTRY CHECKS PASS: $work/report.json; all launched processes terminated"
