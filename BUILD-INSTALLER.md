# Building CK Stem Splitter Setup (method used in this forked repo)
1. Modify CMakeLists.txt if you needs it and builds the JUCE plugin format\standalone with Visual Studio 2026.
2. Installs Python, PyInstaller and "demucs-onnx==0.3.4"
3. Run PyInstaller on ckstem-engine (use this command on cmd: pyinstaller --onefile --name ckstem-engine ckstem_engine.py ) to convert pyton script to executable for Windows.
4. Open .iss file with notepad and write your directories and save it.
5. Packages your plugins and\or standalone, the .exe engine, models as an installer: open .iss with Inno Setup and compile

====================================================================================================================================

# Building CK Stem Splitter Setup (original repo method)

The Windows installer is built by `.github/workflows/build-windows-installer.yml`.

The CI job:

1. Builds the JUCE VST3 with Visual Studio 2022.
2. Installs `demucs-onnx==0.3.0` only on the CI runner.
3. Prewarms the `htdemucs_ft_vocals` FP16-weights model into the staging cache.
4. Freezes `demucs_onnx.cli` with PyInstaller into a portable `ckstem-engine.exe` folder.
5. Packages the VST3, frozen engine, ONNX Runtime dependencies, and model cache with Inno Setup.
6. Uploads `CK-Stem-Splitter-Setup.exe` as the GitHub Actions artifact.

No Python or developer tools are required on the end user's PC.
