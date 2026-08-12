#Requires -Version 5.1
<#
.SYNOPSIS
    Publishes the Windows NSIS installer to GitHub Releases.

.DESCRIPTION
    The counterpart to scripts/release-mac.sh. Where the macOS script gates on signing and
    notarization, this one gates on the payload being complete, because that is the Windows
    equivalent failure: an installer missing platforms\qwindows.dll or a Qt5*.dll runs perfectly on
    the machine that built it - which has Qt on PATH - and dies on every clean machine with "This
    application failed to start because no Qt platform plugin could be initialized". A broken
    release looks fine locally right up until someone else downloads it.

    It also checks the version baked into the packaged ecapplog.exe, which silently drifted from the
    project version between 1.0.4 and 1.1.0.

    Unlike the macOS script, this one attaches to an existing release by default. The macOS build is
    normally published first, and the Windows installer joins the release that is already there; if
    no release exists for the tag, it is created.

    Usually invoked as "cmake --build <dir> --target release --config Release".

.EXAMPLE
    scripts\release-win.ps1 -BuildDir build -VerifyOnly
.EXAMPLE
    scripts\release-win.ps1 -BuildDir build -Yes
#>
[CmdletBinding()]
param(
    # Directory to look for the installer in.
    [string] $BuildDir = 'build',
    # Explicit path to the installer (default: newest ECAppLog-*-win64.exe in the build dir).
    [string] $Exe = '',
    # Release version (default: parsed from the installer file name).
    [string] $Version = '',
    # Git tag to release (default: v<version>).
    [string] $Tag = '',
    # Markdown file to use as the release notes. Only applies when the release is created here;
    # the notes of an existing release are left alone.
    [string] $NotesFile = '',
    # Publish immediately instead of creating a draft. Only applies when creating a release.
    [switch] $Publish,
    # Run the payload checks and stop.
    [switch] $VerifyOnly,
    # Replace an asset of the same name that is already attached to the release.
    [switch] $Clobber,
    # Do not prompt before pushing, tagging and uploading.
    [switch] $Yes
)

# Native tools are checked by exit code throughout: in Windows PowerShell, redirecting a native
# command's stderr turns every line into an ErrorRecord and sets $? to false even on success, so
# 'Stop' here would abort on programs that merely printed a warning.
$ErrorActionPreference = 'Continue'
Set-StrictMode -Version Latest

# Fail instead of blocking on a credential dialog nobody is watching.
$env:GIT_TERMINAL_PROMPT = '0'
if (-not $env:GIT_SSH_COMMAND) { $env:GIT_SSH_COMMAND = 'ssh -o BatchMode=yes' }

function Die    { param([string] $Message) Write-Host "error: $Message" -ForegroundColor Red; exit 1 }
function Info   { param([string] $Message) Write-Host "==> $Message" -ForegroundColor Cyan }
function Indent { param([string] $Text) ($Text -split "`r?`n" | ForEach-Object { "    $_" }) -join "`n" }

# Runs a native command, returning its stdout. Sets $script:LastNativeOk.
function Invoke-Native {
    param([string] $FilePath, [string[]] $CommandArgs, [switch] $Silent)
    $out = & $FilePath @CommandArgs 2>$null
    $script:LastNativeOk = ($LASTEXITCODE -eq 0)
    if (-not $Silent -and $out) { Write-Host (Indent (($out | Out-String).TrimEnd())) }
    return (($out | Out-String).Trim())
}

#
# Preflight
#
if ([System.Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT) {
    Die 'this script only runs on Windows'
}

# Checked up front rather than at upload time: a missing notes file must not surface after the tag
# has already been created and pushed.
if ($NotesFile -and -not (Test-Path -LiteralPath $NotesFile -PathType Leaf)) {
    Die "no such notes file: $NotesFile"
}

#
# Locate the package
#
if (-not $Exe) {
    if (-not (Test-Path -LiteralPath $BuildDir -PathType Container)) {
        Die "no such build directory: $BuildDir"
    }
    # Newest first, so a stale installer from an earlier version is never picked up silently.
    $found = Get-ChildItem -LiteralPath $BuildDir -Filter 'ECAppLog-*-win64.exe' -File |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $found) { Die "no ECAppLog-*-win64.exe found in '$BuildDir' - run cpack first" }
    $Exe = $found.FullName
}
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { Die "no such file: $Exe" }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$exeItem = Get-Item -LiteralPath $Exe
Info "package: $Exe"

#
# Version
#
if (-not $Version) {
    $m = [regex]::Match($exeItem.Name, '^[A-Za-z]+-(\d+\.\d+\.\d+)')
    if (-not $m.Success) {
        Die "could not parse a version from '$($exeItem.Name)' - pass -Version"
    }
    $Version = $m.Groups[1].Value
}
if (-not $Tag) { $Tag = "v$Version" }

#
# Verification gate
#
Info 'checking installer size'
if ($exeItem.Length -lt 1MB) {
    Die "$($exeItem.Name) is only $($exeItem.Length) bytes - CPack produced an empty package.
The most common cause is packaging a config other than Release: both the install() rules and the
deployqt_Release staging directory are Release-only, so a Debug package contains nothing."
}
Write-Host (Indent ("{0:N0} bytes" -f $exeItem.Length))

# CPack leaves the exact tree it compressed behind, which is what gets inspected here. The path is
# <build>\_CPack_Packages\<toplevel tag>\NSIS\<package name>\Main\bin.
Info 'locating staged payload'
$packageName = [IO.Path]::GetFileNameWithoutExtension($Exe)
$stageRoot = Join-Path $BuildDir '_CPack_Packages'
$payload = $null
if (Test-Path -LiteralPath $stageRoot -PathType Container) {
    $payload = Get-ChildItem -LiteralPath $stageRoot -Directory |
        ForEach-Object { Join-Path $_.FullName "NSIS\$packageName\Main\bin" } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
        Select-Object -First 1
}
if (-not $payload) {
    Die "no staged payload for $packageName under '$stageRoot'.
The contents of an NSIS installer cannot be inspected without running it, so this script verifies
the tree CPack compressed. Re-run cpack in '$BuildDir' so the staging directory matches the
installer being released."
}
Write-Host (Indent $payload)

Info 'checking payload contents'
# Qt is linked, not dlopen'd, so a missing DLL fails loudly at startup. qwindows.dll is the
# dangerous one: it is loaded by name at runtime, and its absence is what produces the "no Qt
# platform plugin could be initialized" abort on machines without a Qt installation.
$required = @(
    'ecapplog.exe'
    'Qt5Core.dll'
    'Qt5Gui.dll'
    'Qt5Widgets.dll'
    'Qt5Network.dll'
    'platforms\qwindows.dll'
)
$missing = $required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $payload $_) -PathType Leaf) }
if ($missing) {
    Die "the installer payload is missing:
$(Indent ($missing -join "`n"))
windeployqt did not run or did not stage everything. Reconfigure with
-DECAPPLOG_BUILD_INSTALLER=ON, rebuild the Release config, and re-run cpack."
}
Write-Host (Indent ($required -join "`n"))

Info 'checking embedded version'
$staged = Get-Item -LiteralPath (Join-Path $payload 'ecapplog.exe')
$expected = "$Version.0"
$fileVersion = $staged.VersionInfo.FileVersion
$productVersion = $staged.VersionInfo.ProductVersion
Write-Host (Indent "FileVersion    $fileVersion")
Write-Host (Indent "ProductVersion $productVersion")
if ($fileVersion -ne $expected -or $productVersion -ne $expected) {
    Die "the packaged ecapplog.exe reports $fileVersion / $productVersion, expected $expected.
src\resources\resources.h.in feeds the PE version block and is configured from PROJECT_VERSION, so
a mismatch means the build directory is stale - reconfigure and rebuild it."
}

if ($VerifyOnly) {
    Info "verification passed for $Version ($Tag) - stopping, nothing published"
    exit 0
}

#
# Publishing preconditions
#
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Die 'gh is not installed - run: winget install GitHub.cli, then gh auth login'
}
Invoke-Native gh @('auth', 'status') -Silent | Out-Null
if (-not $LastNativeOk) { Die 'gh is not authenticated - run: gh auth login' }

Invoke-Native git @('diff', '--quiet') -Silent | Out-Null
$dirty = -not $LastNativeOk
Invoke-Native git @('diff', '--cached', '--quiet') -Silent | Out-Null
if ($dirty -or -not $LastNativeOk) {
    Die 'working tree has uncommitted changes - commit them before releasing'
}

$remoteUrl = Invoke-Native git @('remote', 'get-url', 'origin') -Silent
if (-not $LastNativeOk -or -not $remoteUrl) { Die 'no "origin" remote' }
$repo = $remoteUrl -replace '^(git@github\.com:|https://github\.com/)', '' -replace '\.git$', ''
if (-not $repo) { Die "could not derive owner/repo from remote '$remoteUrl'" }

# The branch has to reach origin as well as the tag. Pushing only the tag leaves the release
# pointing at a commit that is not reachable from the branch, so the version bump never shows up in
# the repository history.
$branch = Invoke-Native git @('rev-parse', '--abbrev-ref', 'HEAD') -Silent
if ($branch -eq 'HEAD') { Die 'detached HEAD - check out a branch before releasing' }
Invoke-Native git @('rev-parse', '--abbrev-ref', '--symbolic-full-name', '@{upstream}') -Silent | Out-Null
if (-not $LastNativeOk) {
    Die "branch '$branch' has no upstream - push it once with: git push -u origin $branch"
}
$unpushed = [int](Invoke-Native git @('rev-list', '--count', '@{upstream}..HEAD') -Silent)

# Checked before anything is created: git falls back to an interactive credential prompt when it
# cannot authenticate, and the prompts are invisible under a build tool, so an unreachable origin
# would otherwise surface as a hang between tagging and uploading.
Info 'checking origin is reachable'
Invoke-Native git @('ls-remote', '--exit-code', 'origin', 'HEAD') -Silent | Out-Null
if (-not $LastNativeOk) {
    Die "cannot reach origin non-interactively at $remoteUrl.
For an SSH remote, check that a key is loaded (ssh -T git@github.com). To use gh's credentials over
HTTPS instead: gh auth setup-git, then point origin at https://github.com/$repo.git"
}

$tagExists = $false
Invoke-Native git @('rev-parse', '-q', '--verify', "refs/tags/$Tag") -Silent | Out-Null
if ($LastNativeOk) {
    $tagCommit = Invoke-Native git @('rev-list', '-n1', $Tag) -Silent
    $headCommit = Invoke-Native git @('rev-parse', 'HEAD') -Silent
    if ($tagCommit -ne $headCommit) {
        # Not fatal the way it is on macOS. The Windows installer is routinely built after the tag,
        # from a commit that carries a Windows-only fix, and refusing here would block the exact
        # workflow this script exists to automate.
        Write-Host "warning: tag $Tag does not point at HEAD - the installer is built from $($headCommit.Substring(0,8)), the tag is at $($tagCommit.Substring(0,8))" -ForegroundColor Yellow
    }
    $tagExists = $true
}

$releaseJson = Invoke-Native gh @('release', 'view', $Tag, '--repo', $repo, '--json', 'assets,isDraft') -Silent
$releaseExists = $LastNativeOk
$assetExists = $false
if ($releaseExists) {
    $release = $releaseJson | ConvertFrom-Json
    $assetExists = @($release.assets | Where-Object { $_.name -eq $exeItem.Name }).Count -gt 0
    if ($assetExists -and -not $Clobber) {
        Die "release $Tag already has an asset named $($exeItem.Name) - pass -Clobber to replace it"
    }
}

#
# Confirm, then publish
#
if (-not $Yes) {
    if ($unpushed -gt 0) { $branchNote = "$unpushed commit(s) will be pushed" }
    else                 { $branchNote = 'already up to date with origin' }

    if ($releaseExists) {
        if ($release.isDraft) { $state = 'existing draft release' } else { $state = 'existing published release' }
        if ($assetExists)     { $action = "replace $($exeItem.Name)" } else { $action = "attach $($exeItem.Name)" }
        $tagNote = "$Tag (exists)"
    } else {
        if ($Publish) { $state = 'new release, published immediately' } else { $state = 'new release, draft' }
        $action = "attach $($exeItem.Name)"
        if ($tagExists) { $tagNote = "$Tag, pushed to origin" } else { $tagNote = "$Tag (will be created), pushed to origin" }
    }

    Write-Host ''
    Write-Host "About to release to ${repo}:"
    Write-Host "  branch : $branch ($branchNote)"
    Write-Host "  tag    : $tagNote"
    Write-Host "  asset  : $Exe"
    Write-Host "  action : $action"
    Write-Host "  target : $state"
    Write-Host ''
    $reply = Read-Host 'Continue? [y/N]'
    if ($reply -notmatch '^(y|yes)$') { Die 'aborted' }
}

# Branch before tag, deliberately: if the branch push is rejected (non-fast-forward, no permission)
# the release aborts here, rather than after a tag has already reached origin.
if ($unpushed -gt 0) {
    Info "pushing $branch to origin ($unpushed commit(s))"
    & git push origin $branch
    if ($LASTEXITCODE -ne 0) { Die "failed to push $branch" }
}

if (-not $releaseExists) {
    if (-not $tagExists) {
        Info "creating tag $Tag"
        & git tag -a $Tag -m "ECAppLog $Version"
        if ($LASTEXITCODE -ne 0) { Die "failed to create tag $Tag" }
    }
    Info "pushing $Tag to origin"
    & git push origin $Tag
    if ($LASTEXITCODE -ne 0) { Die "failed to push tag $Tag" }

    $releaseArgs = @('release', 'create', $Tag, '--repo', $repo, '--title', "ECAppLog $Version")
    if ($NotesFile) { $releaseArgs += @('--notes-file', $NotesFile) }
    else            { $releaseArgs += @('--notes', 'Windows installer (64-bit).') }
    if (-not $Publish) { $releaseArgs += '--draft' }

    Info "creating release $Tag"
    & gh @releaseArgs $Exe
    if ($LASTEXITCODE -ne 0) { Die "failed to create release $Tag" }

    if (-not $Publish) {
        Info 'draft created - review it, then publish with:'
        Write-Host "        gh release edit $Tag --repo $repo --draft=false"
    } else {
        Info "released $Tag"
    }
} else {
    $uploadArgs = @('release', 'upload', $Tag, $Exe, '--repo', $repo)
    if ($Clobber) { $uploadArgs += '--clobber' }

    Info "uploading $($exeItem.Name) to release $Tag"
    & gh @uploadArgs
    if ($LASTEXITCODE -ne 0) { Die "failed to upload to release $Tag" }
    Info "attached to $Tag - https://github.com/$repo/releases/tag/$Tag"
}
