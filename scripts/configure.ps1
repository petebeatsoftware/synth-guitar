# Configures (and optionally builds) the project using the x64 MSVC toolset.
#
# Running CMake directly from a plain PowerShell window selects the 32-bit
# host compiler by default, which conflicts with the 64-bit JUCE/Windows SDK
# libraries this project links against. This script loads the x64 Visual
# Studio developer environment first, then runs CMake, so manual builds
# behave the same way Visual Studio's own CMake integration does.
#
# Usage:
#   scripts/configure.ps1 [-Preset <name>] [-Build] [-Test]

param(
	[string]$Preset = "dev",
	[switch]$Build,
	[switch]$Test
)

$ErrorActionPreference = "Stop"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
	throw "vswhere.exe not found at '$vswhere'. Install Visual Studio with the C++ workload."
}

$vsInstallPath = & $vswhere -latest -products * `
	-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
	-property installationPath
if (-not $vsInstallPath) {
	throw "Could not find a Visual Studio installation with the x64 C++ toolset."
}

$vcvars64 = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars64)) {
	throw "vcvars64.bat not found at '$vcvars64'."
}

$repoRoot = Split-Path -Parent $PSScriptRoot

function Invoke-InVsDevShell([string]$CommandLine) {
	$wrapped = '"' + $vcvars64 + '" >nul && ' + $CommandLine
	cmd /c $wrapped
	if ($LASTEXITCODE -ne 0) {
		throw "Command failed with exit code $LASTEXITCODE`: $CommandLine"
	}
}

Invoke-InVsDevShell "cmake --preset $Preset -S `"$repoRoot`""

if ($Build) {
	Invoke-InVsDevShell "cmake --build --preset $Preset"
}

if ($Test) {
	Invoke-InVsDevShell "ctest --preset $Preset"
}
