# Convert test files from printf to kprintf
$testFiles = Get-ChildItem -Path "tests\unit\test_*.c"

foreach ($file in $testFiles) {
    $content = Get-Content $file.FullName -Raw
    
    # Replace printf with kprintf
    $content = $content -replace '\bprintf\(', 'kprintf('
    
    # Replace main() with run_*_tests()
    $testName = $file.BaseName -replace 'test_', ''
    $content = $content -replace 'int main\(void\)', "void run_${testName}_tests(void)"
    $content = $content -replace 'return \(test_passed == test_count\) \? 0 : 1;', ''
    $content = $content -replace 'return 0;$', ''
    
    # Copy to kernel/tests
    $destPath = "kernel\tests\$($file.Name)"
    Set-Content -Path $destPath -Value $content
    
    Write-Host "Converted $($file.Name)"
}

Write-Host "All test files converted!"
