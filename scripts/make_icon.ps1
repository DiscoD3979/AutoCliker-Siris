Add-Type -AssemblyName System.Drawing

$out = "D:\Project Code\Projects\AutoCliker-Siris\icon.ico"

# мастер-картинка 256x256
$m = New-Object System.Drawing.Bitmap 256, 256
$g = [System.Drawing.Graphics]::FromImage($m)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)

# тёмный скруглённый квадрат с жёлтой рамкой
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$d = 56; $x = 8; $y = 8; $w = 240; $h = 240
$path.AddArc($x, $y, $d, $d, 180, 90)
$path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
$path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
$path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
$path.CloseFigure()
$bg = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 20, 20, 26))
$g.FillPath($bg, $path)
$bd = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 209, 64)), 7
$g.DrawPath($bd, $path)

# курсор-стрелка
$pts = [System.Drawing.PointF[]]@(
    (New-Object System.Drawing.PointF 92, 58),
    (New-Object System.Drawing.PointF 92, 176),
    (New-Object System.Drawing.PointF 126, 148),
    (New-Object System.Drawing.PointF 152, 208),
    (New-Object System.Drawing.PointF 178, 196),
    (New-Object System.Drawing.PointF 152, 138),
    (New-Object System.Drawing.PointF 196, 136)
)
$ab = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 209, 64))
$g.FillPolygon($ab, $pts)

# кольцо "клика" справа снизу
$rp = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 209, 64)), 9
$g.DrawEllipse($rp, 128, 128, 96, 96)
$r2 = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 209, 64)), 5
$g.DrawEllipse($r2, 156, 156, 40, 40)
$g.Dispose()

# даунскейл во все размеры и сборка ICO (PNG-сжатые кадры)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngs = @()
foreach ($s in $sizes) {
    $b2 = New-Object System.Drawing.Bitmap $s, $s
    $g2 = [System.Drawing.Graphics]::FromImage($b2)
    $g2.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g2.DrawImage($m, 0, 0, $s, $s)
    $ms = New-Object System.IO.MemoryStream
    $b2.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += , $ms.ToArray()
    $g2.Dispose(); $b2.Dispose(); $ms.Dispose()
}
$m.Dispose()

$fs = New-Object System.IO.FileStream ($out, [System.IO.FileMode]::Create)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([uint16]0)          # reserved
$bw.Write([uint16]1)          # type: icon
$bw.Write([uint16]$pngs.Count)
$off = 6 + 16 * $pngs.Count
for ($i = 0; $i -lt $pngs.Count; $i++) {
    $s = $sizes[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([byte]$dim)
    $bw.Write([byte]$dim)
    $bw.Write([byte]0)        # palette
    $bw.Write([byte]0)        # reserved
    $bw.Write([uint16]1)      # planes
    $bw.Write([uint16]32)     # bpp
    $bw.Write([uint32]$pngs[$i].Length)
    $bw.Write([uint32]$off)
    $off += $pngs[$i].Length
}
foreach ($png in $pngs) { $bw.Write($png) }
$bw.Close(); $fs.Close()
Write-Host "icon.ico written: $((Get-Item $out).Length) bytes"