function Controller()
{
    installer.setValue("ProductUUID", "{6f6238e2-0bb2-4bb1-a77e-6f3a61a9fabc}");
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
    installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
    installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
}
