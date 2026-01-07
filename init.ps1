
# PowerShell Integration for AI Shell
# Source this file in your profile: . "path\to\init.ps1"

$AiExePath = Join-Path $PSScriptRoot "bin\ai.exe"
$Global:AiShellUsedSession = $false

# Wrapper function to track usage
function Global:ai {
    $Global:AiShellUsedSession = $true
    & $AiExePath @args
}

function Global:Clear-Host {
    if ($Global:AiShellUsedSession) {
        # Silent clear history
        & $AiExePath --clear-history | Out-Null
        $Global:AiShellUsedSession = $false
    }
    [System.Console]::Clear()
}

function Global:cls {
    Clear-Host
}

function Global:clear {
    Clear-Host
}
