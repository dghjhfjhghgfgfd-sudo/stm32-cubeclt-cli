# Jetson TX2 VNC Remote Desktop

## Verified Case

Host PC and Jetson were on the same WLAN. Jetson IP was:

```text
10.230.104.117
```

Windows RealVNC Viewer can connect with:

```text
10.230.104.117:5900
```

Password used in this bring-up:

```text
nvidia
```

## Discovery

Find Jetson when the IP changes:

```powershell
ipconfig
arp -a
```

Scan current subnet for SSH if needed:

```powershell
$jobs=@(); 1..254 | % { $ip="10.230.104.$_"; $jobs += Start-Job -ScriptBlock { param($ip) $c=New-Object Net.Sockets.TcpClient; $iar=$c.BeginConnect($ip,22,$null,$null); if($iar.AsyncWaitHandle.WaitOne(350,$false)){ try{$c.EndConnect($iar); if($c.Connected){$ip}}catch{} } $c.Close() } -ArgumentList $ip }; Receive-Job -Job $jobs -Wait; Remove-Job -Job $jobs
```

## Prefer Existing Vino Before Installing x11vnc

On this TX2, apt is dirty because some NVIDIA/OpenCV/TensorRT packages are partially configured. Do not run `apt-get -f install` casually.

Check first:

```bash
which x11vnc vino-server vncserver 2>/dev/null || true
apt-cache policy vino x11vnc
ss -lntp | grep 590 || true
```

This board already had:

```text
/usr/lib/vino/vino-server
```

## Enable Vino

Get the graphical session environment:

```bash
ps -u nvidia -f | grep unity-settings-daemon | grep -v grep
tr '\0' '\n' < /proc/<PID>/environ | egrep 'DBUS_SESSION_BUS_ADDRESS|DISPLAY|XAUTHORITY'
```

For the verified session:

```bash
export DISPLAY=:0
export XAUTHORITY=/home/nvidia/.Xauthority
export DBUS_SESSION_BUS_ADDRESS=unix:abstract=/tmp/dbus-JX1n9zm86C

gsettings set org.gnome.Vino enabled true
gsettings set org.gnome.Vino prompt-enabled false
gsettings set org.gnome.Vino require-encryption false
gsettings set org.gnome.Vino authentication-methods "['vnc']"
gsettings set org.gnome.Vino vnc-password 'bnZpZGlh'

nohup /usr/lib/vino/vino-server > /home/nvidia/vino.log 2>&1 &
```

`bnZpZGlh` is base64 for `nvidia`.

## Autostart After Desktop Login

Create:

```text
/home/nvidia/.config/autostart/vino-server.desktop
```

Content:

```ini
[Desktop Entry]
Type=Application
Name=Vino VNC Server
Exec=/usr/lib/vino/vino-server
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
Comment=Start VNC desktop sharing for Jetson TX2
```

## Verify From Windows

```powershell
Test-NetConnection -ComputerName 10.230.104.117 -Port 5900
```

Expected:

```text
TcpTestSucceeded : True
```

## Notes

- RealVNC Viewer `Add device` is for cloud-managed devices. For LAN use, type `IP:5900` in the top search/address bar.
- Vino on old Ubuntu may require `require-encryption false` for RealVNC compatibility.
- If there is no desktop session on `:0`, Vino cannot share it. Use HDMI/login or switch to a virtual VNC server approach.
