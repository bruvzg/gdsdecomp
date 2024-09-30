# check if the args are less than 1
if ($args.Length -lt 1) {
    $script_name = $MyInvocation.MyCommand.Name
    echo "Usage: $script_name <export_preset> [-cmd <export_command>] [-path <export_path>]"
    exit 1
}

$export_preset = $args[0]

$export_command = ""
$export_path = ""

# check if the args are more than 1
if ($args.Length -gt 1) {
    for ($i = 1; $i -lt $args.Length; $i++) {
        $arg = $args[$i]
        switch ($arg) {
            "-cmd" {
                $i++
                $export_command = $args[$i]
            }
            "-path" {
                $i++
                $export_path = $args[$i]
            }
            default {
                echo "Unknown argument: $arg"
                exit 1
            }
        }
    }
}

# get the current script path
$scriptPath = $MyInvocation.MyCommand.Path
$scriptDir = Split-Path $scriptPath
$scriptDir = $scriptDir -replace '\\', '/'
$standaloneDir = $scriptDir -replace '/.scripts', '/standalone'
# we're in ${workspaceDir}/modules/gdsdecomp/.scripts, need to be in ${workspaceDir}/bin
$godotBinDir = $scriptDir -replace '/modules/gdsdecomp/.scripts', '/bin'

# check if $export_command is empty
if ($export_command -eq "") {
    $godotEditor = Get-ChildItem $godotBinDir -Filter "*editor*" -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($godotEditor -eq $null) {
        echo "Godot editor path not given and not found in $godotBinDir"
        exit 1
    }
    # get the path of the godot executable
    $export_command = $godotEditor.FullName
}

cd $standaloneDir
mkdir -p .export
# replace only the first 'v' (the long abbreviation may have 'v' in it, and we don't want to replace that)
# So, instead of using "-replace 'v', ''", we use "-replace 'v', '', 1"

$version = $(git describe --tags --abbrev=6) -replace '^v', ''

# $version = "0.6.2"
# split at the first '-' and take the first part
$number_only_version = $version -split '-', 2 | Select-Object -First 1
# if the version is like 0.6.2-42-g17f56f, we want the number in-between the '-' (e.g. 42)
$build_num = $version -split '-', 3 | Select-Object -Index 1
if ($build_num -eq $null) {
    $build_num = 0
}
$version = $version -replace 'v', ''


echo "Exporting standalone project"
echo "Godot editor binary: $export_command"
echo "Export preset: $export_preset"
$echoed_path = $export_path
if ($export_path -eq "") {
    $echoed_path = "<DEFAULT>"
}
echo "Export path: $echoed_path"
echo "Version: $version"
echo "Number only version: $number_only_version"
echo "Build number: $build_num"

$export_presets = Get-Content export_presets.cfg
$export_presets = $export_presets -replace 'application/short_version=".*"', "application/short_version=""$version"""
$export_presets = $export_presets -replace 'application/version=".*"', "application/version=""$version""" 
$export_presets = $export_presets -replace 'application/file_version=".*"', "application/file_version=""$number_only_version.$build_num"""
$export_presets = $export_presets -replace 'application/product_version=".*"', "application/product_version=""$number_only_version.$build_num"""

#output the processed export_presets.cfg
$export_presets | Set-Content export_presets.cfg


$export_release_args = "--export-release `"$export_preset`" $export_name"
$proc = Start-Process -NoNewWindow -PassThru -FilePath "$export_command" -ArgumentList '--headless -e --quit'
Wait-Process -Id $proc.id -Timeout 300
$proc = Start-Process -NoNewWindow -PassThru -FilePath "$export_command" -ArgumentList "--headless $export_release_args"
Wait-Process -Id $proc.id -Timeout 300
