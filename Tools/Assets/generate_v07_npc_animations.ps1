param(
    [string]$MakeHumanRoot = (Join-Path $env:LOCALAPPDATA 'makehuman-community')
)

$ErrorActionPreference = 'Stop'
$workspaceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$scratchRoot = Join-Path $workspaceRoot '.codex_tmp\makehuman-v07'
$suitsRoot = Join-Path $workspaceRoot '.codex_tmp\makehuman\suits03'
$pluginSource = Join-Path $PSScriptRoot 'MakeHumanPlugin\9_ws_autogen'

if (-not (Test-Path -LiteralPath $suitsRoot)) {
    $commonGitDir = (& git -C $workspaceRoot rev-parse --path-format=absolute --git-common-dir).Trim()
    $sharedCache = Join-Path (Split-Path -Parent $commonGitDir) '.codex_tmp\makehuman\suits03'
    if (Test-Path -LiteralPath $sharedCache) {
        $suitsRoot = $sharedCache
    }
    else {
        & python -X utf8 (Join-Path $PSScriptRoot 'download_makehuman_inputs.py')
        if ($LASTEXITCODE -ne 0) {
            throw 'MakeHuman input download failed.'
        }
    }
}

$pythonExe = Join-Path $MakeHumanRoot 'Python\python.exe'
$wrapper = Join-Path $MakeHumanRoot 'mhstartwrapper.py'
$pluginsRoot = Join-Path $MakeHumanRoot 'makehuman\plugins'
foreach ($required in @($pythonExe, $wrapper, $pluginsRoot, $pluginSource, $suitsRoot)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required MakeHuman pipeline path is missing: $required"
    }
}
Copy-Item -LiteralPath $pluginSource -Destination $pluginsRoot -Recurse -Force

$jobs = @(
    @{ Character = 'engineer'; Folder = 'Engineer'; Prefix = 'WS_Engineer' },
    @{ Character = 'doctor'; Folder = 'Doctor'; Prefix = 'WS_Doctor' }
)
$suffixes = @('Walk', 'Acknowledge', 'Consider', 'Reassure', 'Reject', 'Alarmed')

try {
    $env:WS_MH_AUTOGEN = '1'
    $env:WS_MH_SUITS_ROOT = $suitsRoot
    foreach ($job in $jobs) {
        $temporaryFolder = Join-Path $scratchRoot $job.Folder
        New-Item -ItemType Directory -Path $temporaryFolder -Force | Out-Null
        $env:WS_MH_CHARACTER = $job.Character
        $env:WS_MH_OUTPUT = Join-Path $temporaryFolder ($job.Prefix + '.fbx')

        Push-Location $MakeHumanRoot
        try {
            & $pythonExe $wrapper
            if ($LASTEXITCODE -ne 0) {
                throw "MakeHuman exited with code $LASTEXITCODE for $($job.Character)."
            }
        }
        finally {
            Pop-Location
        }

        $destination = Join-Path $workspaceRoot (Join-Path 'SourceAssets\MakeHuman\AnimationsV07' $job.Folder)
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        foreach ($suffix in $suffixes) {
            $source = Join-Path $temporaryFolder ($job.Prefix + '_' + $suffix + '.fbx')
            if (-not (Test-Path -LiteralPath $source)) {
                throw "MakeHuman did not produce animation: $source"
            }
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
        Write-Host "Generated v0.7 animations for $($job.Character): $destination"
    }
}
finally {
    Remove-Item Env:WS_MH_AUTOGEN -ErrorAction SilentlyContinue
    Remove-Item Env:WS_MH_CHARACTER -ErrorAction SilentlyContinue
    Remove-Item Env:WS_MH_OUTPUT -ErrorAction SilentlyContinue
    Remove-Item Env:WS_MH_SUITS_ROOT -ErrorAction SilentlyContinue
}
