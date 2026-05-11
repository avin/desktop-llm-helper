function quoteForPowerShell(value)
{
    return "'" + String(value).split("'").join("''") + "'";
}

function removeOldUninstallEntries()
{
    var targetDir = installer.toNativeSeparators(installer.value("TargetDir"));
    var command =
        "$targetDir = [System.IO.Path]::GetFullPath(" + quoteForPowerShell(targetDir) + "); " +
        "$normalizedTargetDir = $targetDir.TrimEnd([char]92); " +
        "$root = 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall'; " +
        "if (Test-Path $root) { " +
        "Get-ChildItem $root | ForEach-Object { " +
        "$item = Get-ItemProperty $_.PSPath; " +
        "$displayName = [string]$item.DisplayName; " +
        "if ($displayName -eq 'DesktopLLMHelper' -or $displayName -eq 'Desktop LLM Helper') { " +
        "$matchesTarget = $false; " +
        "if ($item.InstallLocation) { " +
        "try { " +
        "$installLocation = [System.IO.Path]::GetFullPath([string]$item.InstallLocation).TrimEnd([char]92); " +
        "$matchesTarget = $installLocation -ieq $normalizedTargetDir; " +
        "} catch {} " +
        "} " +
        "if (-not $matchesTarget -and $item.UninstallString) { " +
        "$matchesTarget = ([string]$item.UninstallString).IndexOf($targetDir, [System.StringComparison]::OrdinalIgnoreCase) -ge 0; " +
        "} " +
        "if ($matchesTarget) { Remove-Item $_.PSPath -Recurse -Force; } " +
        "} " +
        "} " +
        "}";

    try {
        installer.execute("powershell.exe", [
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            command
        ]);
    } catch(e) {
        console.log("Failed to remove old DesktopLLMHelper uninstall entries: " + e);
    }
}

function Component()
{
    installer.finishButtonClicked.connect(this, Component.prototype.launchApplication);
}

Component.prototype.launchApplication = function()
{
    try {
        if (installer.isInstaller() && installer.status === QInstaller.Success) {
            var targetDir = installer.value("TargetDir");
            var exePath = installer.toNativeSeparators(targetDir + "/DesktopLLMHelper.exe");
            installer.executeDetached(exePath, [], installer.toNativeSeparators(targetDir));
        }
    } catch(e) {
        console.log("Failed to launch application: " + e);
    }
}

Component.prototype.createOperations = function()
{
    // Stop running instance of the application before copying files
    if (systemInfo.productType === "windows") {
        try {
            installer.execute("taskkill", ["/F", "/IM", "DesktopLLMHelper.exe"]);
        } catch(e) {
            console.log("Failed to terminate running DesktopLLMHelper: " + e);
        }

        removeOldUninstallEntries();
    }
    // perform the default file-install operations
    component.createOperations();

    // on Windows, add a shortcut to the Startup folder for autorun
    if (systemInfo.productType === "windows") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/DesktopLLMHelper.exe",
            "@UserStartMenuProgramsPath@/Startup/DesktopLLMHelper.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/DesktopLLMHelper.exe"
        );
    }
}
