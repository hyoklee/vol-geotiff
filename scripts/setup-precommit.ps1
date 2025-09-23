param(
    [switch]$Install
)

# Ensure Python and pre-commit are available
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Error "Python is required to install pre-commit. Please install Python first."
    exit 1
}

if (-not (Get-Command pip -ErrorAction SilentlyContinue)) {
    Write-Error "pip is required to install pre-commit. Please ensure Python/pip are on PATH."
    exit 1
}

# Install pre-commit if requested or missing
if ($Install -or -not (Get-Command pre-commit -ErrorAction SilentlyContinue)) {
    Write-Output "Installing pre-commit..."
    pip install --user pre-commit || exit 1
    $userBase = python -c "import site; print(site.USER_BASE)"
    $precommitPath = Join-Path $userBase "Scripts"
    if ($env:Path -notlike "*${precommitPath}*") {
        Write-Warning "pre-commit may not be on PATH. Consider adding: $precommitPath"
    }
}

# Install git hook
Write-Output "Installing pre-commit git hooks..."
pre-commit install || exit 1

Write-Output "Running pre-commit on all files..."
pre-commit run --all-files

Write-Output "Done."
