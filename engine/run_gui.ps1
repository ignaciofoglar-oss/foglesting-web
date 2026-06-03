$ErrorActionPreference = "Stop"

Get-Process sheet_nest_gui -ErrorAction SilentlyContinue | Stop-Process -Force
.\build.ps1

Start-Process -FilePath (Resolve-Path -LiteralPath "build\sheet_nest_gui.exe").Path
