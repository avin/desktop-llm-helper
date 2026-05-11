param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$RootDir,

    [datetime]$ReleaseDate = (Get-Date)
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RootDir)) {
    $scriptRoot = $PSScriptRoot
    if ([string]::IsNullOrWhiteSpace($scriptRoot)) {
        $scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
    }

    $RootDir = (Resolve-Path (Join-Path $scriptRoot "..")).Path
}

if ($Version -notmatch '^[0-9]+([.-][0-9]+)*$') {
    throw "Installer version '$Version' must match Qt IFW format: digits separated by dots or hyphens."
}

function Read-XmlDocument {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $document = [System.Xml.XmlDocument]::new()
    $document.PreserveWhitespace = $true
    $document.Load($Path)
    return $document
}

function Set-XmlElementTextInFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$XPath,

        [Parameter(Mandatory = $true)]
        [string]$ElementName,

        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $document = Read-XmlDocument -Path $Path
    $node = $document.SelectSingleNode($XPath)
    if ($null -eq $node) {
        throw "Required XML node '$XPath' was not found."
    }

    $content = [System.IO.File]::ReadAllText($Path)
    $regex = [regex]::new("(?s)(<$ElementName(?:\s+[^>]*)?>)(.*?)(</$ElementName>)")
    $match = $regex.Match($content)
    if (-not $match.Success) {
        throw "XML element '$ElementName' was not found in '$Path'."
    }

    $escapedValue = [System.Security.SecurityElement]::Escape($Value)
    $newContent = $content.Substring(0, $match.Groups[2].Index) +
        $escapedValue +
        $content.Substring($match.Groups[2].Index + $match.Groups[2].Length)

    [System.IO.File]::WriteAllText($Path, $newContent, [System.Text.UTF8Encoding]::new($false))
}

$configPath = Join-Path $RootDir "installer\config\config.xml"
$packagePath = Join-Path $RootDir "installer\packages\com.desktopllmhelper\meta\package.xml"

Set-XmlElementTextInFile -Path $configPath -XPath "/Installer/Version" -ElementName "Version" -Value $Version
Set-XmlElementTextInFile -Path $packagePath -XPath "/Package/Version" -ElementName "Version" -Value $Version
Set-XmlElementTextInFile -Path $packagePath -XPath "/Package/ReleaseDate" -ElementName "ReleaseDate" -Value $ReleaseDate.ToString("yyyy-MM-dd")

Write-Host "Prepared installer metadata for version $Version."
