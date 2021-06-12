$srcDir =	[String]$pwd + "\src\"		# Directory for source files
$buildDir =	[String]$pwd + "\build\"	# Ouput directory
$outFile =	"Renderer.exe"				# Executable name

$compilerFlags =
	"-g",
	"-std=c17",
	"-Wall",
	"-Wextra",
	"-Werror",
	"-Wno-unused-parameter",
	"-Wno-unused-variable",
	"-lUser32.lib",
	"-lGdi32.lib",
	("-l" + $Env:VK_SDK_PATH + "/Lib/vulkan-1.lib"),
	("-I" + $Env:VK_SDK_PATH + "\Include")
$compilerDefines =
	"-D_CRT_SECURE_NO_WARNINGS",
	"-D_DEBUG"

$files = [System.Collections.ArrayList]@() # Create empty array
foreach ($file in Get-ChildItem $srcDir -Recurse -Include "*.c" -Force) { # For each file in the source directory
	$unused = $files.Add($file.FullName)									# Add it to the list
}

Write-Output Compiling: @files # Output the files that we are compiling to the console

if (!(Test-Path buildDir)) {								# If the build directory does not exist
	$unused = New-Item $buildDir -ItemType Directory -Force	# Create it
}

Push-Location $buildDir										# Go into the build directory
clang @compilerDefines @compilerFlags -o $outFile @files -static	# Build the project
Pop-Location												# Exit the build directory

glslangValidator.exe .\triangle.vert.glsl -V -o .\triangle.vert.spirv
glslangValidator.exe .\triangle.frag.glsl -V -o .\triangle.frag.spirv
