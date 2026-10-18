# ZuCompleluxe - Native Folder Selector with "Patch Folder" Button
param(
    [string]$InitialPath = ""
)

# If path is provided via CLI argument and exists, return immediately (for CLI/scripts)
if (![string]::IsNullOrWhiteSpace($InitialPath) -and (Test-Path $InitialPath)) {
    Write-Output (Resolve-Path $InitialPath).Path
    exit 0
}

# Native Windows File Explorer Folder Picker via IFileOpenDialog
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class NativeFolderPicker {
    [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int SHCreateItemFromParsingName(
        [MarshalAs(UnmanagedType.LPWStr)] string pszPath,
        IntPtr pbc,
        ref Guid riid,
        [MarshalAs(UnmanagedType.Interface)] out IShellItem ppv);

    [ComImport]
    [Guid("d57c7288-d4ad-4768-be02-9d969532d960")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IFileOpenDialog {
        [PreserveSig] int Show(IntPtr parent);
        [PreserveSig] int SetFileTypes(uint cFileTypes, IntPtr rgFilterSpec);
        [PreserveSig] int SetFileTypeIndex(uint iFileType);
        [PreserveSig] int GetFileTypeIndex(out uint piFileType);
        [PreserveSig] int Advise(IntPtr pfde, out uint pdwCookie);
        [PreserveSig] int Unadvise(uint dwCookie);
        [PreserveSig] int SetOptions(uint fos);
        [PreserveSig] int GetOptions(out uint fos);
        [PreserveSig] int SetDefaultFolder(IShellItem psi);
        [PreserveSig] int SetFolder(IShellItem psi);
        [PreserveSig] int GetFolder(out IShellItem ppsi);
        [PreserveSig] int GetCurrentSelection(out IShellItem ppsi);
        [PreserveSig] int SetFileName([MarshalAs(UnmanagedType.LPWStr)] string pszName);
        [PreserveSig] int GetFileName([MarshalAs(UnmanagedType.LPWStr)] out string pszName);
        [PreserveSig] int SetTitle([MarshalAs(UnmanagedType.LPWStr)] string pszTitle);
        [PreserveSig] int SetOkButtonLabel([MarshalAs(UnmanagedType.LPWStr)] string pszText);
        [PreserveSig] int SetFileNameLabel([MarshalAs(UnmanagedType.LPWStr)] string pszLabel);
        [PreserveSig] int GetResult(out IShellItem ppsi);
    }

    [ComImport]
    [Guid("43826d1e-e718-42ee-bc55-a1e261c37bfe")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IShellItem {
        [PreserveSig] int BindToHandler(IntPtr pbc, ref Guid bhid, ref Guid riid, out IntPtr ppv);
        [PreserveSig] int GetParent(out IShellItem ppsi);
        [PreserveSig] int GetDisplayName(uint sigdnName, [MarshalAs(UnmanagedType.LPWStr)] out string ppszName);
        [PreserveSig] int GetAttributes(uint sfgaoMask, out uint psfgaoAttribs);
        [PreserveSig] int Compare(IShellItem psi, uint hint, out int piOrder);
    }

    [ComImport]
    [Guid("DC1C5A9C-E88A-4dde-A5A1-60F82A20AEF7")]
    [ClassInterface(ClassInterfaceType.None)]
    private class FileOpenDialogRCW { }

    public static string PickFolder(string initialDir, string title, string okLabel) {
        var dialog = (IFileOpenDialog)new FileOpenDialogRCW();
        // FOS_PICKFOLDERS = 0x20, FOS_FORCEFILESYSTEM = 0x40, FOS_PATHMUSTEXIST = 0x800
        uint fos = 0x00000020 | 0x00000040 | 0x00000800;
        dialog.SetOptions(fos);
        if (!string.IsNullOrEmpty(title)) dialog.SetTitle(title);
        if (!string.IsNullOrEmpty(okLabel)) dialog.SetOkButtonLabel(okLabel);

        if (!string.IsNullOrEmpty(initialDir) && System.IO.Directory.Exists(initialDir)) {
            Guid riid = new Guid("43826d1e-e718-42ee-bc55-a1e261c37bfe");
            IShellItem folder;
            if (SHCreateItemFromParsingName(initialDir, IntPtr.Zero, ref riid, out folder) == 0 && folder != null) {
                dialog.SetFolder(folder);
            }
        }

        int hr = dialog.Show(IntPtr.Zero);
        if (hr == 0) {
            IShellItem result;
            if (dialog.GetResult(out result) == 0 && result != null) {
                string path;
                result.GetDisplayName(0x80058000, out path); // SIGDN_FILESYSPATH
                return path;
            }
        }
        return null;
    }
}
"@ -ErrorAction SilentlyContinue

$startDir = if ([string]::IsNullOrWhiteSpace($InitialPath)) { [System.Environment]::CurrentDirectory } else { $InitialPath }
$selectedPath = [NativeFolderPicker]::PickFolder($startDir, "Select Zuma Deluxe Game Installation Folder to Patch", "Patch Folder")

if ([string]::IsNullOrWhiteSpace($selectedPath)) {
    exit 1
}

if (-not (Test-Path $selectedPath)) {
    exit 1
}

$zumaExe = Join-Path $selectedPath "Zuma.exe"
$zumaBak = Join-Path $selectedPath "Zuma.exe.bak"
$zumaModBak = Join-Path $selectedPath "modules\bak\Zuma.exe"
$zumaModOrig = Join-Path $selectedPath "modules\bak\Zuma.exe.orig"
$zumaModBase = Join-Path $selectedPath "modules\bak\Zuma.base.exe"
$otherExes = Get-ChildItem -Path $selectedPath -Filter "*.exe" -File -ErrorAction SilentlyContinue | Where-Object { $_.Length -ge 300KB }

if (-not (Test-Path $zumaExe) -and -not (Test-Path $zumaBak) -and -not (Test-Path $zumaModBak) -and -not (Test-Path $zumaModOrig) -and -not (Test-Path $zumaModBase) -and (-not $otherExes)) {
    Add-Type -AssemblyName System.Windows.Forms
    $res = [System.Windows.Forms.MessageBox]::Show(
        "Executable Zuma Deluxe tidak ditemukan di folder:`n$selectedPath`n`nApakah Anda yakin ingin tetap mem-patch folder ini?",
        "Konfirmasi Folder Zuma",
        [System.Windows.Forms.MessageBoxButtons]::YesNo,
        [System.Windows.Forms.MessageBoxIcon]::Warning
    )
    if ($res -ne [System.Windows.Forms.DialogResult]::Yes) {
        exit 1
    }
}

Write-Output (Resolve-Path $selectedPath).Path
exit 0
