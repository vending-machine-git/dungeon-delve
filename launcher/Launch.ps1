param(
    [string]$GamePath,
    [string]$WorkshopRoot,
    [string]$FrameworkContent,
    [string]$DelveContent,
    [ValidateSet('Delve', 'Framework')][string]$Mode = 'Delve',
    [switch]$CheckOnly,
    [switch]$PrepareOnly,
    [string]$ProfileRoot,
    # Steam invite code (steam:<lobby>) or lobby id to join once the game opens.
    [string]$ConnectLobby
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)

function Read-Json([string]$Path) {
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
}

function Find-Content([string]$Id, [string]$ExplicitPath, [string[]]$SearchRoots, [string]$Version) {
    $candidates = @()
    if ($ExplicitPath) {
        $candidates = @(Get-Item -LiteralPath $ExplicitPath)
    } else {
        foreach ($searchRoot in $SearchRoots) {
            if (Test-Path -LiteralPath $searchRoot -PathType Container) {
                $candidates += @(Get-ChildItem -LiteralPath $searchRoot -Directory)
            }
        }
    }
    foreach ($candidate in $candidates) {
        $metadataPath = Join-Path $candidate.FullName 'mod.json'
        if (!(Test-Path -LiteralPath $metadataPath -PathType Leaf)) { continue }
        try { $metadata = Read-Json $metadataPath } catch {
            if ($ExplicitPath) { throw }
            continue
        }
        if (!$metadata.PSObject.Properties['id'] -or !$metadata.PSObject.Properties['version'] -or !$metadata.PSObject.Properties['api_version']) { continue }
        if ($metadata.id -eq $Id -and $metadata.version -eq $Version -and $metadata.api_version -eq 1) {
            foreach ($entry in @(Get-Item -LiteralPath $candidate.FullName) + @(Get-ChildItem -LiteralPath $candidate.FullName -Recurse -Force)) {
                if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                    throw "Content must not contain links or junctions: $($entry.FullName)"
                }
            }
            return $candidate.FullName
        }
    }
    throw "Missing $Id content version $Version (API 1). Extract the complete matching Dungeon Delve ZIP, or pass its content folder explicitly. Legacy Workshop users should update both subscriptions."
}

try {
    $release = Read-Json (Join-Path $PSScriptRoot 'release.json')
    $binary = Join-Path $PSScriptRoot 'runtime\barony-rmf.exe'
    if ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ne $release.runtime_sha256) {
        throw 'Runtime checksum mismatch. Extract a fresh official release; do not mix versions.'
    }
    $libraries = @()
    $steamPath = $null
    foreach ($registryPath in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam')) {
        $registry = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
        if ($registry) {
            $property = $registry.PSObject.Properties['SteamPath']
            if (!$property) { $property = $registry.PSObject.Properties['InstallPath'] }
            if ($property) { $steamPath = [string]$property.Value; break }
        }
    }
    if ($steamPath) {
        $libraries += $steamPath
        $libraryFile = Join-Path $steamPath 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraryFile) {
            $libraryText = Get-Content -LiteralPath $libraryFile -Raw
            foreach ($match in [regex]::Matches($libraryText, '"path"\s+"([^"]+)"')) {
                $libraries += $match.Groups[1].Value.Replace('\\', '\')
            }
        }
    }
    if (!$GamePath) {
        $adjacentGame = Split-Path $PSScriptRoot -Parent
        if (Test-Path -LiteralPath (Join-Path $adjacentGame 'barony.exe') -PathType Leaf) {
            $GamePath = $adjacentGame
        }
    }
    if (!$GamePath) {
        foreach ($library in $libraries) {
            $candidate = Join-Path $library 'steamapps\common\Barony'
            if (Test-Path -LiteralPath (Join-Path $candidate 'barony.exe')) { $GamePath = $candidate; break }
        }
    }
    if (!$GamePath) { throw 'Cannot locate Barony. Run Launch.ps1 -GamePath "your Steam Barony folder".' }
    if (!(Test-Path -LiteralPath $GamePath -PathType Container) -or
        !(Test-Path -LiteralPath (Join-Path $GamePath 'barony.exe') -PathType Leaf)) {
        throw 'Choose the Barony game folder containing barony.exe. Use Choose Barony folder to locate it, or pass -GamePath when using the script.'
    }
    $GamePath = (Get-Item -LiteralPath $GamePath).FullName
    if ((Get-FileHash -LiteralPath (Join-Path $GamePath 'barony.exe') -Algorithm SHA256).Hash -ne $release.game_sha256) {
        throw 'This Barony build is not the tested Steam Windows 5.0.2 build. Verify Steam files or obtain a matching framework release. Nothing was overwritten.'
    }
    $bundledMods = Join-Path $PSScriptRoot 'mods'
    if ($release.PSObject.Properties['bundled_content'] -or (Test-Path -LiteralPath $bundledMods -PathType Container)) {
        if (!$FrameworkContent) { $FrameworkContent = Join-Path $bundledMods 'vending_machine_framework' }
        if (!$DelveContent) { $DelveContent = Join-Path $bundledMods 'extraction-raid' }
    }
    $searchRoots = @()
    if ($WorkshopRoot) { $searchRoots += $WorkshopRoot }
    $searchRoots += Join-Path $GamePath 'mods'
    $searchRoots += Join-Path (Split-Path (Split-Path $GamePath -Parent) -Parent) 'workshop\content\371970'
    foreach ($library in $libraries) { $searchRoots += Join-Path $library 'steamapps\workshop\content\371970' }
    $frameworkFolder = Find-Content 'vending_machine_framework' $FrameworkContent $searchRoots $release.version
    $folders = @($frameworkFolder)
    if ($Mode -eq 'Delve') {
        $delveFolder = Find-Content 'extraction-raid' $DelveContent $searchRoots $release.version
        $delveMetadata = Read-Json (Join-Path $delveFolder 'mod.json')
        if ($delveMetadata.requires.vending_machine_framework -ne 1) { throw 'Invalid Dungeon Delve framework dependency.' }
        $folders += $delveFolder
    }
    if ($release.PSObject.Properties['bundled_content']) {
        foreach ($content in $release.bundled_content) {
            $bundledFolder = Join-Path $bundledMods $content.id
            if ($folders -notcontains $bundledFolder) { continue }
            $expectedFiles = @($content.files | ForEach-Object { $_.path })
            $actualFiles = @(Get-ChildItem -LiteralPath $bundledFolder -File -Recurse -Force | ForEach-Object {
                $_.FullName.Substring($bundledFolder.Length + 1).Replace('\', '/')
            })
            if (@(Compare-Object $expectedFiles $actualFiles).Count -ne 0) {
                throw "Bundled content files differ from the release: $($content.id). Extract a fresh complete ZIP."
            }
            foreach ($file in $content.files) {
                $contentPath = Join-Path $bundledFolder $file.path
                if ((Get-FileHash -LiteralPath $contentPath -Algorithm SHA256).Hash -ne $file.sha256) {
                    throw "Bundled content checksum mismatch: $($content.id)/$($file.path). Extract a fresh complete ZIP."
                }
            }
        }
    }
    foreach ($dependency in $release.game_dlls) {
        $dependencyPath = Join-Path $GamePath $dependency.name
        if ((Get-FileHash -LiteralPath $dependencyPath -Algorithm SHA256).Hash -ne $dependency.sha256) {
            throw "Missing or incompatible game dependency: $($dependency.name). Verify Steam files."
        }
    }
    if (!$ProfileRoot) { $ProfileRoot = Join-Path $env:LOCALAPPDATA 'vending_machine_framework' }
    $ProfileRoot = [IO.Path]::GetFullPath($ProfileRoot)
    foreach ($protectedFolder in @($GamePath) + $folders) {
        $protectedPath = [IO.Path]::GetFullPath($protectedFolder).TrimEnd('\')
        if ($ProfileRoot -eq $protectedPath -or $ProfileRoot.StartsWith($protectedPath + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'ProfileRoot must be outside the retail game and content folders.'
        }
    }
    $report = [ordered]@{ version = $release.version; protocol = $release.protocol; mode = $Mode; game = $GamePath; content = $folders; profile_root = $ProfileRoot }
    if ($CheckOnly) { $report | ConvertTo-Json -Depth 5; exit 0 }
    $session = Join-Path $ProfileRoot ($release.version + '\sessions\' + [guid]::NewGuid().ToString('N'))
    $runtimeFolder = Join-Path $session 'runtime'
    $profile = Join-Path $session 'profile'
    New-Item -ItemType Directory -Path $runtimeFolder, $profile -Force | Out-Null
    Copy-Item -LiteralPath $binary -Destination (Join-Path $runtimeFolder 'barony-rmf.exe')
    foreach ($dependency in $release.game_dlls) {
        Copy-Item -LiteralPath (Join-Path $GamePath $dependency.name) -Destination $runtimeFolder
    }
    foreach ($folder in $folders) {
        foreach ($directory in @('data', 'maps', 'images', 'music')) {
            $source = Join-Path $folder $directory
            if (Test-Path -LiteralPath $source -PathType Container) {
                $destination = Join-Path $profile $directory
                New-Item -ItemType Directory -Path $destination -Force | Out-Null
                foreach ($entry in Get-ChildItem -LiteralPath $source -Force) {
                    Copy-Item -LiteralPath $entry.FullName -Destination $destination -Recurse -Force
                }
            }
        }
    }
    $arguments = @('-windowed', '-size=1280x720', ('-datadir=' + $GamePath))
    if ($Mode -eq 'Delve') { $arguments += '-raid-profile=' + (Join-Path $profile 'delve-profile.json') }
    if ($ConnectLobby) {
        $lobbyText = $ConnectLobby.Trim()
        if ($lobbyText -match '^(?i:steam:)\s*') { $lobbyText = $lobbyText -replace '^(?i:steam:)\s*', '' }
        [UInt64]$lobbyId = 0
        if (!($lobbyText -match '^\d+$') -or ![UInt64]::TryParse($lobbyText, [ref]$lobbyId) -or $lobbyId -eq 0) {
            throw 'Invalid Steam invite code: ' + $ConnectLobby
        }
        $arguments += @('+connect_lobby', [string]$lobbyId)
        $report['connect_lobby'] = [string]$lobbyId
    }
    $report['profile'] = $profile
    $report['binary'] = Join-Path $runtimeFolder 'barony-rmf.exe'
    if (!$PrepareOnly) {
        $env:PATH = $GamePath + [IO.Path]::PathSeparator + $env:PATH
        $quotedArguments = @($arguments | ForEach-Object { '"' + $_ + '"' })
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = $report.binary
        $startInfo.Arguments = $quotedArguments -join ' '
        $startInfo.WorkingDirectory = $profile
        $startInfo.UseShellExecute = $true
        $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
        $process = [System.Diagnostics.Process]::Start($startInfo)
        $report['pid'] = $process.Id
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $session 'launch.json') -Encoding UTF8
    $report | ConvertTo-Json -Depth 5
    exit 0
} catch {
    [Console]::Error.WriteLine('Launch failed: ' + $_.Exception.Message)
    exit 1
}
