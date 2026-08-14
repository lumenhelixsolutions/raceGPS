# raceGPS Installer UI Mock (PowerShell Windows Forms)
# Run this to see *exactly* how the NSIS custom pages (Preflight + Finish) will look and behave.
# This replicates the nsDialogs pages from installer\racegps-setup.nsi as closely as possible.
# Uses the same text, colors, logic, and button actions.
# Brand accent: Electric Route Blue (#00CCFF) where it fits.

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = 'SilentlyContinue'

# Constants matching the .nsi
$MIN_RAM_MB = 8192
$MIN_DISK_MB = 5120
$PRODUCT_VERSION = "0.2.0"
$PRODUCT_NAME = "raceGPS"

$formWidth = 520
$formHeight = 420

function New-InstallerForm {
    param([string]$Title)
    $f = New-Object System.Windows.Forms.Form
    $f.Text = "$PRODUCT_NAME $PRODUCT_VERSION Setup"
    $f.Size = New-Object System.Drawing.Size($formWidth, $formHeight)
    $f.StartPosition = 'CenterScreen'
    $f.FormBorderStyle = 'FixedDialog'
    $f.MaximizeBox = $false
    $f.MinimizeBox = $false
    $f.BackColor = [System.Drawing.Color]::White
    $f.Font = New-Object System.Drawing.Font('Segoe UI', 9)
    return $f
}

function Add-MUIHeader {
    param($form, [string]$MainText, [string]$SubText)
    # Simulate MUI header area (light blue bar at top like classic NSIS MUI)
    $header = New-Object System.Windows.Forms.Panel
    $header.Location = New-Object System.Drawing.Point(0, 0)
    $header.Size = New-Object System.Drawing.Size($formWidth, 48)
    $header.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)  # typical MUI blue

    $lblMain = New-Object System.Windows.Forms.Label
    $lblMain.Text = $MainText
    $lblMain.ForeColor = [System.Drawing.Color]::White
    $lblMain.Font = New-Object System.Drawing.Font('Segoe UI', 11, [System.Drawing.FontStyle]::Bold)
    $lblMain.Location = New-Object System.Drawing.Point(12, 8)
    $lblMain.AutoSize = $true
    $header.Controls.Add($lblMain)

    $lblSub = New-Object System.Windows.Forms.Label
    $lblSub.Text = $SubText
    $lblSub.ForeColor = [System.Drawing.Color]::FromArgb(200, 220, 255)
    $lblSub.Font = New-Object System.Drawing.Font('Segoe UI', 8.5)
    $lblSub.Location = New-Object System.Drawing.Point(14, 28)
    $lblSub.AutoSize = $true
    $header.Controls.Add($lblSub)

    $form.Controls.Add($header)
    return $header
}

# ============================================================
# PREFLIGHT PAGE (exact replica of PreflightPage function)
# ============================================================
function Show-PreflightPage {
    $f = New-InstallerForm
    Add-MUIHeader $f "System Check" "raceGPS will verify your system is ready."

    # Title
    $title = New-Object System.Windows.Forms.Label
    $title.Text = "Pre-Flight Checklist"
    $title.Font = New-Object System.Drawing.Font('Segoe UI', 12, [System.Drawing.FontStyle]::Bold)
    $title.Location = New-Object System.Drawing.Point(16, 60)
    $title.Size = New-Object System.Drawing.Size(480, 24)
    $f.Controls.Add($title)

    # We'll populate these dynamically with real checks (same logic as .nsi)
    $y = 95
    $labelHeight = 18
    $gap = 20

    # OS row
    $osLabel = New-Object System.Windows.Forms.Label
    $osLabel.Location = New-Object System.Drawing.Point(16, $y)
    $osLabel.Size = New-Object System.Drawing.Size(480, $labelHeight)
    $f.Controls.Add($osLabel)
    $y += $gap

    # RAM row
    $ramLabel = New-Object System.Windows.Forms.Label
    $ramLabel.Location = New-Object System.Drawing.Point(16, $y)
    $ramLabel.Size = New-Object System.Drawing.Size(480, $labelHeight)
    $f.Controls.Add($ramLabel)
    $y += $gap

    # Disk row
    $diskLabel = New-Object System.Windows.Forms.Label
    $diskLabel.Location = New-Object System.Drawing.Point(16, $y)
    $diskLabel.Size = New-Object System.Drawing.Size(480, $labelHeight)
    $f.Controls.Add($diskLabel)
    $y += $gap

    # DirectX row
    $dxLabel = New-Object System.Windows.Forms.Label
    $dxLabel.Location = New-Object System.Drawing.Point(16, $y)
    $dxLabel.Size = New-Object System.Drawing.Size(480, $labelHeight)
    $f.Controls.Add($dxLabel)
    $y += $gap

    # VC++ row
    $vcLabel = New-Object System.Windows.Forms.Label
    $vcLabel.Location = New-Object System.Drawing.Point(16, $y)
    $vcLabel.Size = New-Object System.Drawing.Size(480, $labelHeight)
    $f.Controls.Add($vcLabel)
    $y += 28

    # GPU hint (multi-line, wrapped)
    $gpuLabel = New-Object System.Windows.Forms.Label
    $gpuLabel.Text = "If you have a discrete GPU (NVIDIA/AMD) with 6GB+ VRAM, we recommend High graphics settings. Integrated graphics will use Low settings automatically."
    $gpuLabel.Location = New-Object System.Drawing.Point(16, $y)
    $gpuLabel.Size = New-Object System.Drawing.Size(480, 48)
    $gpuLabel.ForeColor = [System.Drawing.Color]::FromArgb(90, 90, 90)
    $f.Controls.Add($gpuLabel)

    # Perform the exact same checks as the NSIS script
    $canProceed = $true

    # OS
    $winMajor = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -ErrorAction SilentlyContinue).CurrentMajorVersionNumber
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
    if ($winMajor -ge 10) {
        $osLabel.Text = "✓ Windows 10/11 detected"
        $osLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 170, 0)   # 0x00AA00
    } else {
        $osLabel.Text = "✗ Windows 10/11 required. Upgrade to continue."
        $osLabel.ForeColor = [System.Drawing.Color]::Red
        $canProceed = $false
    }

    # RAM (match the GlobalMemoryStatusEx math in spirit)
    $cs = Get-CimInstance Win32_ComputerSystem -ErrorAction SilentlyContinue
    $ramMB = [math]::Round($cs.TotalPhysicalMemory / 1MB)
    if ($ramMB -ge $MIN_RAM_MB) {
        $ramLabel.Text = "✓ RAM: $ramMB MB (min: ${MIN_RAM_MB} MB)"
        $ramLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 170, 0)
    } else {
        $ramLabel.Text = "⚠ RAM: $ramMB MB (min: ${MIN_RAM_MB} MB recommended)"
        $ramLabel.ForeColor = [System.Drawing.Color]::FromArgb(255, 165, 0)
    }

    # Disk (simulates the $INSTDIR or ProgramFiles64 check; we use C: for demo)
    $drive = 'C:'
    $disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$drive'" -ErrorAction SilentlyContinue
    $freeMB = [math]::Round($disk.FreeSpace / 1MB)
    if ($freeMB -ge $MIN_DISK_MB) {
        $diskLabel.Text = "✓ Disk space: $freeMB MB available on $drive"
        $diskLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 170, 0)
    } else {
        $diskLabel.Text = "✗ Disk space: $freeMB MB (need ${MIN_DISK_MB} MB). Choose another drive."
        $diskLabel.ForeColor = [System.Drawing.Color]::Red
        $canProceed = $false
    }

    # DirectX
    $dxVer = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\DirectX' -ErrorAction SilentlyContinue).Version
    if ($dxVer) {
        $dxLabel.Text = "✓ DirectX runtime present"
        $dxLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 170, 0)
    } else {
        $dxLabel.Text = "⚠ DirectX version unknown. Runtime will install if needed."
        $dxLabel.ForeColor = [System.Drawing.Color]::FromArgb(255, 165, 0)
    }

    # VC++
    $vcVer = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64' -ErrorAction SilentlyContinue).Version
    if ($vcVer) {
        $vcLabel.Text = "✓ VC++ Redistributables present"
        $vcLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 170, 0)
    } else {
        $vcLabel.Text = "⚠ VC++ 2015-2022 Redistributable will be installed."
        $vcLabel.ForeColor = [System.Drawing.Color]::FromArgb(255, 165, 0)
    }

    # Bottom buttons (standard NSIS positions)
    $btnBack = New-Object System.Windows.Forms.Button
    $btnBack.Text = "< Back"
    $btnBack.Location = New-Object System.Drawing.Point(240, 360)
    $btnBack.Size = New-Object System.Drawing.Size(75, 28)
    $btnBack.Enabled = $false   # first custom page in flow
    $f.Controls.Add($btnBack)

    $btnNext = New-Object System.Windows.Forms.Button
    $btnNext.Text = "Next >"
    $btnNext.Location = New-Object System.Drawing.Point(325, 360)
    $btnNext.Size = New-Object System.Drawing.Size(75, 28)
    $btnNext.Enabled = $canProceed
    if (-not $canProceed) { $btnNext.BackColor = [System.Drawing.Color]::LightGray }
    $btnNext.Add_Click({ 
        $f.Close()
        Show-ComponentsPage   # simulate advancing
    })
    $f.Controls.Add($btnNext)

    $btnCancel = New-Object System.Windows.Forms.Button
    $btnCancel.Text = "Cancel"
    $btnCancel.Location = New-Object System.Drawing.Point(420, 360)
    $btnCancel.Size = New-Object System.Drawing.Size(75, 28)
    $btnCancel.Add_Click({ $f.Close() })
    $f.Controls.Add($btnCancel)

    # Small footer note
    $footer = New-Object System.Windows.Forms.Label
    $footer.Text = "This is a visual + functional mock of the real NSIS PreflightPage. The actual installer uses the identical checks and colors."
    $footer.Font = New-Object System.Drawing.Font('Segoe UI', 7.5, [System.Drawing.FontStyle]::Italic)
    $footer.ForeColor = [System.Drawing.Color]::Gray
    $footer.Location = New-Object System.Drawing.Point(16, 330)
    $footer.Size = New-Object System.Drawing.Size(480, 20)
    $f.Controls.Add($footer)

    [void]$f.ShowDialog()
}

# ============================================================
# Fake Components page (to show flow)
# ============================================================
function Show-ComponentsPage {
    $f = New-InstallerForm
    Add-MUIHeader $f "Choose Components" "Select the components you want to install."

    $lbl = New-Object System.Windows.Forms.Label
    $lbl.Text = "Components:"
    $lbl.Font = New-Object System.Drawing.Font('Segoe UI', 10, [System.Drawing.FontStyle]::Bold)
    $lbl.Location = New-Object System.Drawing.Point(16, 60)
    $f.Controls.Add($lbl)

    $items = @(
        "Game Files (required) — the full raceGPS executable + data",
        "Akron Citypack (Default) — bundled city data for immediate play",
        "Visual C++ Redistributables — auto-install if missing",
        "Desktop Shortcut",
        "Start Menu Shortcuts"
    )
    $y = 90
    foreach ($item in $items) {
        $cb = New-Object System.Windows.Forms.CheckBox
        $cb.Text = $item
        $cb.Location = New-Object System.Drawing.Point(20, $y)
        $cb.Size = New-Object System.Drawing.Size(470, 20)
        $cb.Checked = $true
        if ($item -like "*required*") { $cb.Enabled = $false }
        $f.Controls.Add($cb)
        $y += 26
    }

    $note = New-Object System.Windows.Forms.Label
    $note.Text = "(The real installer marks Game Files and Akron Citypack as read-only/always installed.)"
    $note.Font = New-Object System.Drawing.Font('Segoe UI', 8, [System.Drawing.FontStyle]::Italic)
    $note.ForeColor = [System.Drawing.Color]::Gray
    $note.Location = New-Object System.Drawing.Point(16, 230)
    $note.Size = New-Object System.Drawing.Size(480, 30)
    $f.Controls.Add($note)

    $btnBack = New-Object System.Windows.Forms.Button
    $btnBack.Text = "< Back"
    $btnBack.Location = New-Object System.Drawing.Point(240, 360)
    $btnBack.Size = New-Object System.Drawing.Size(75, 28)
    $btnBack.Add_Click({ $f.Close(); Show-PreflightPage })
    $f.Controls.Add($btnBack)

    $btnNext = New-Object System.Windows.Forms.Button
    $btnNext.Text = "Next >"
    $btnNext.Location = New-Object System.Drawing.Point(325, 360)
    $btnNext.Size = New-Object System.Drawing.Size(75, 28)
    $btnNext.Add_Click({ $f.Close(); Show-FakeProgress })
    $f.Controls.Add($btnNext)

    $btnCancel = New-Object System.Windows.Forms.Button
    $btnCancel.Text = "Cancel"
    $btnCancel.Location = New-Object System.Drawing.Point(420, 360)
    $btnCancel.Size = New-Object System.Drawing.Size(75, 28)
    $btnCancel.Add_Click({ $f.Close() })
    $f.Controls.Add($btnCancel)

    [void]$f.ShowDialog()
}

function Show-FakeProgress {
    $f = New-InstallerForm
    Add-MUIHeader $f "Installing..." "Please wait while raceGPS is being installed."

    $prog = New-Object System.Windows.Forms.ProgressBar
    $prog.Location = New-Object System.Drawing.Point(30, 120)
    $prog.Size = New-Object System.Drawing.Size(450, 22)
    $prog.Style = 'Continuous'
    $prog.Minimum = 0
    $prog.Maximum = 100
    $f.Controls.Add($prog)

    $status = New-Object System.Windows.Forms.Label
    $status.Text = "Copying game files..."
    $status.Location = New-Object System.Drawing.Point(30, 155)
    $status.Size = New-Object System.Drawing.Size(450, 20)
    $f.Controls.Add($status)

    $timer = New-Object System.Windows.Forms.Timer
    $timer.Interval = 120
    $i = 0
    $timer.Add_Tick({
        $i += 4
        if ($i -gt 100) { $i = 100 }
        $prog.Value = $i
        if ($i -lt 30) { $status.Text = "Copying game files..." }
        elseif ($i -lt 55) { $status.Text = "Installing citypacks..." }
        elseif ($i -lt 75) { $status.Text = "Registering shortcuts and uninstaller..." }
        elseif ($i -lt 92) { $status.Text = "Installing Visual C++ Redistributable (if needed)..." }
        else { $status.Text = "Finalizing installation..." }

        if ($i -ge 100) {
            $timer.Stop()
            $f.Close()
            Show-FinishPage
        }
    })
    $timer.Start()

    [void]$f.ShowDialog()
}

# ============================================================
# FINISH PAGE (exact replica of FinishPage + Launch functions)
# ============================================================
function Show-FinishPage {
    $f = New-InstallerForm
    Add-MUIHeader $f "Setup Complete" "raceGPS is ready to race."

    $complete = New-Object System.Windows.Forms.Label
    $complete.Text = "Installation Complete!"
    $complete.Font = New-Object System.Drawing.Font('Segoe UI', 13, [System.Drawing.FontStyle]::Bold)
    $complete.Location = New-Object System.Drawing.Point(16, 70)
    $complete.Size = New-Object System.Drawing.Size(480, 28)
    $f.Controls.Add($complete)

    $desc = New-Object System.Windows.Forms.Label
    $desc.Text = "raceGPS has been installed successfully. On first launch, the game will run a quick hardware check and guide you through controller setup, graphics settings, and your first race."
    $desc.Location = New-Object System.Drawing.Point(16, 110)
    $desc.Size = New-Object System.Drawing.Size(470, 70)
    $desc.ForeColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
    $f.Controls.Add($desc)

    # Launch game button (big, prominent)
    $btnLaunch = New-Object System.Windows.Forms.Button
    $btnLaunch.Text = "Launch raceGPS"
    $btnLaunch.Font = New-Object System.Drawing.Font('Segoe UI', 10, [System.Drawing.FontStyle]::Bold)
    $btnLaunch.Location = New-Object System.Drawing.Point(60, 200)
    $btnLaunch.Size = New-Object System.Drawing.Size(380, 36)
    $btnLaunch.BackColor = [System.Drawing.Color]::FromArgb(0, 204, 255)  # Electric Route Blue accent
    $btnLaunch.ForeColor = [System.Drawing.Color]::Black
    $btnLaunch.Add_Click({
        $f.Close()
        try {
            # In the real installer this does: Exec '"$INSTDIR\raceGPS.exe"'
            Start-Process "notepad.exe" -ArgumentList "This would launch raceGPS.exe in the real installer. (Mock)"
        } catch {}
    })
    $f.Controls.Add($btnLaunch)

    # Citypack manager button
    $btnCity = New-Object System.Windows.Forms.Button
    $btnCity.Text = "Open Citypack Manager"
    $btnCity.Font = New-Object System.Drawing.Font('Segoe UI', 9)
    $btnCity.Location = New-Object System.Drawing.Point(60, 250)
    $btnCity.Size = New-Object System.Drawing.Size(380, 32)
    $btnCity.Add_Click({
        $f.Close()
        try {
            # Real: Exec '"$INSTDIR\raceGPS.exe" -citypackmanager'
            Start-Process "notepad.exe" -ArgumentList "This would launch raceGPS.exe -citypackmanager (mock)"
        } catch {}
    })
    $f.Controls.Add($btnCity)

    $note = New-Object System.Windows.Forms.Label
    $note.Text = "These buttons match the real FinishPage in racegps-setup.nsi exactly."
    $note.Font = New-Object System.Drawing.Font('Segoe UI', 7.5, [System.Drawing.FontStyle]::Italic)
    $note.ForeColor = [System.Drawing.Color]::Gray
    $note.Location = New-Object System.Drawing.Point(16, 310)
    $note.Size = New-Object System.Drawing.Size(480, 18)
    $f.Controls.Add($note)

    $btnClose = New-Object System.Windows.Forms.Button
    $btnClose.Text = "Close"
    $btnClose.Location = New-Object System.Drawing.Point(380, 360)
    $btnClose.Size = New-Object System.Drawing.Size(100, 28)
    $btnClose.Add_Click({ $f.Close() })
    $f.Controls.Add($btnClose)

    [void]$f.ShowDialog()
}

# ============================================================
# Entry point
# ============================================================
Write-Host "raceGPS Installer UI Mock" -ForegroundColor Cyan
Write-Host "This will open actual Windows dialogs that replicate the NSIS preflight + finish pages." -ForegroundColor Gray
Write-Host "Close the windows when you're done looking." -ForegroundColor Gray
Write-Host ""

Show-PreflightPage

Write-Host "Mock closed. Run the script again anytime." -ForegroundColor DarkGray
