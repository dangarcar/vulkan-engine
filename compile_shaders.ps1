param (
    [string]$SourceDir = ".",
    [string]$Compiler = "glslc"
)

if (-not (Get-Command $Compiler -ErrorAction SilentlyContinue)) {
    Write-Error "Shader compiler '$Compiler' not found in PATH."
    return
}

$extensions = @("*.frag", "*.comp", "*.vert")
$shaderFiles = Get-ChildItem -Path $SourceDir -Recurse -Include $extensions -File | Where-Object { $_.FullName -notmatch "[\\\/]build[\\\/]" }

foreach ($file in $shaderFiles) {
    $shaderDir = $file.Directory.FullName
    $binDir = Join-Path $shaderDir "bin"
    
    if (-not (Test-Path $binDir)) {
        New-Item -ItemType Directory -Path $binDir | Out-Null
    }
    
    $outputName = $file.Name + ".spv"
    $outputPath = Join-Path $binDir $outputName
    
    Write-Host "Compiling $($file.FullName) -> $outputPath"
    
    & $Compiler $file.FullName -o $outputPath
    
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to compile: $($file.FullName)"
    }
}

Write-Host "Shader compilation completed!"