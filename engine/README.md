# Foglesting Engine

Codigo fuente del motor C++ de Foglesting para escritorio y WebAssembly.

Esta carpeta esta pensada para que un colaborador pueda compilar, probar y
modificar el programa sin depender de rutas locales de la maquina de Ignacio.

## Que contiene

- `src/core`: lectura/escritura DXF, geometria, solver de nesting y logging.
- `src/gui`: interfaz Win32 nativa del descargable.
- `src/wasm`: binding del motor para la version online.
- `src/cli`: ejecutable de consola para pruebas y automatizacion.
- `tests`: tests del motor.
- `tools`: scripts auxiliares de build/tuning.
- `web`: plantilla de la version online. Los binarios `nesting.js` y
  `nesting.wasm` se generan al compilar WebAssembly y se publican desde
  `website/online`.

## Compilar escritorio en Windows

Requisitos:

- CMake
- Ninja
- Toolchain C++ compatible con Windows, o los wrappers de Zig incluidos en
  `tools/` si estan configurados en tu maquina.

```powershell
cd engine
.\build.ps1
```

Ejecutar GUI local:

```powershell
cd engine
.\run_gui.ps1
```

Ejecutar tests:

```powershell
cd engine
ctest --test-dir build --output-on-failure
```

## Compilar WebAssembly

Requisitos:

- Emscripten instalado localmente.
- Activar el entorno de Emscripten antes de correr CMake.

Ejemplo:

```powershell
cd engine
emcmake cmake -S . -B build_wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_wasm --target nesting_wasm
```

Despues de compilar, copiar:

- `build_wasm/nesting.js` a `../online/nesting.js`
- `build_wasm/nesting.wasm` a `../online/nesting.wasm`

## Tuning

El script `tools/tune_nesting.py` no trae rutas locales hardcodeadas. Podes
pasar la carpeta DXF por argumento o con la variable:

```powershell
$env:FOGLESTING_TUNE_DXF_DIR = "C:\ruta\a\DXF"
python tools\tune_nesting.py --help
```

## Importante

- No subir credenciales ni JSON de Firebase.
- No subir builds locales, `.exe`, `.zip`, `.pdb`, `build/`, `build_wasm/` o SDKs.
- El ejecutable publico de la pagina vive en `downloads/` y no se toca salvo
  cuando Ignacio aprueba una release.
