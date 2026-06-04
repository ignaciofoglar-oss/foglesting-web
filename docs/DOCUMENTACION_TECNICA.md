# Foglesting — Documentación técnica

> Documento de arquitectura y diseño del sistema. Audiencia: equipo técnico,
> integradores y evaluación de producción.
> Última actualización: 2026-06-04.

---

## 1. Visión general

**Foglesting** es un sistema de optimización de corte de chapa (*nesting*) compuesto por:

- Un **motor de cálculo nativo en C++** (algoritmo genético + geometría + I/O de DXF).
- Una **interfaz de escritorio Win32** para Windows.
- Una **versión web** del motor compilada a **WebAssembly** (corre en el navegador).
- Un **conversor STEP → DXF** que despliega piezas de chapa a su patrón plano.
- Un **backend web** (sitio + panel de administración + API) sobre Vercel + Firebase.

El mismo núcleo de C++ se reutiliza en el escritorio y en la web, lo que garantiza
resultados de nesting consistentes entre ambas plataformas.

```
                         ┌────────────────────────────┐
                         │     nesting_core (C++)      │
                         │  dxf · geometry · nesting   │
                         └─────┬───────────────┬───────┘
            ┌──────────────────┘               └───────────────────┐
   ┌────────▼─────────┐                              ┌─────────────▼───────────┐
   │  GUI Win32 (.exe) │                              │ WASM (navegador, online) │
   └────────┬──────────┘                              └─────────────────────────┘
            │ (botón "Agregar STEP")
   ┌────────▼───────────────────────┐      ┌──────────────────────────────────┐
   │ Conversor STEP→DXF (Python+OCC)│      │ Backend: Vercel + Firebase        │
   │ desplegado de chapa            │      │ auth · licencia · releases ·      │
   └────────────────────────────────┘      │ diagnóstico DXF · admin           │
                                           └──────────────────────────────────┘
```

---

## 2. Componentes y código fuente

| Componente | Ubicación | Lenguaje |
|---|---|---|
| Núcleo de nesting | `src/core/` (`dxf`, `geometry`, `nesting`, `perf_log`, `gpu_context`) | C++20 |
| CLI | `src/cli/main.cpp` | C++20 |
| GUI de escritorio | `src/gui/win32_app.cpp` | C++20 / Win32 |
| Binding WebAssembly | `src/wasm/wasm_binding.cpp` | C++ / Emscripten |
| Conversor STEP→DXF | `step2dxf/unfold.py`, `verify.py` | Python + OpenCASCADE |
| Sitio y API | `website/` (`api/*.js`, `admin.*`, `index.html`) | JS (Node/Vercel) + HTML/CSS |

Build con **CMake** (ver §9).

---

## 3. Motor de nesting (algoritmo genético)

El nesting es un problema de empaquetamiento NP-difícil. Foglesting lo resuelve con una
**meta-heurística evolutiva (algoritmo genético)** combinada con técnicas geométricas.

### 3.1 Representación
Cada **solución candidata** es un ordenamiento (secuencia) de las piezas junto con la
rotación elegida para cada una. Las piezas se colocan secuencialmente sobre la(s) chapa(s)
buscando la mejor posición válida.

### 3.2 Colocación geométrica
- Se evalúan **posiciones candidatas** respetando la **separación mínima** entre piezas y los
  bordes de la chapa.
- Soporta **rotaciones discretas** configurables (p. ej. cada 90° o cada 45°).
- Una etapa opcional de **compactación** desplaza las piezas para cerrar huecos.

### 3.3 Ciclo evolutivo
1. **Población inicial:** se generan varias soluciones (parámetro *GA population*).
2. **Evaluación (fitness):** se mide el aprovechamiento / área ocupada según el criterio elegido.
3. **Mutación y reordenamiento:** se alteran secuencias y rotaciones (*Mutation %*, *Shuffle %*,
   *Intensity %*) para explorar el espacio de soluciones y escapar de óptimos locales.
4. **Selección:** se conservan las mejores soluciones por iteración.
5. Se repite durante *N* **iteraciones**; se conserva la **mejor solución histórica**.

### 3.4 Criterios de optimización
- **Bounding Box:** minimiza el rectángulo envolvente ocupado.
- **Compact Area:** busca un empaquetado más cerrado; suele rendir mejor en piezas irregulares.
- `optimization_ratio` balancea internamente compactación vs. criterio de ocupación.

### 3.5 Resultado
La salida (`NestingResult`) contiene: piezas colocadas (con posición, rotación, índice de chapa),
**piezas no ubicadas**, cantidad de chapas, área usada/material y **utilización (%)**. Las piezas
que no entran en la chapa se reportan explícitamente al usuario.

### 3.6 Paralelismo
El solver usa múltiples **núcleos de CPU** (parámetro *CPU cores*). La iteración es observable en
tiempo real mediante un *callback* de progreso (usado por la GUI para graficar la convergencia).

---

## 4. Lectura y escritura de DXF

### 4.1 Entidades soportadas (lectura)
`LINE`, `ARC`, `CIRCLE`, `LWPOLYLINE`, `POLYLINE`, `SPLINE`, `ELLIPSE`, además de `INSERT`
(bloques) con sus transformaciones.

### 4.2 Reconstrucción de contornos
- Las **polilíneas/círculos/elipses cerradas** se toman como polígonos directamente.
- Las **líneas/arcos sueltos** se **cosen** en bucles cerrados por coincidencia de extremos
  (con una tolerancia configurable, *Endpoint tol.*), reparando pequeñas separaciones.
- Se identifica el **contorno exterior** (mayor área) y los **agujeros** internos por contención.

### 4.3 Unidades
El factor de escala de importación (**mm / pulgadas**) se aplica a la geometría. Una unidad
incorrecta es la causa más común de piezas mal escaladas.

### 4.4 Exportación
El layout optimizado se exporta como DXF, listo para corte. Soporta unión de líneas de corte
comunes entre piezas adyacentes (*Merge common*).

---

## 5. Desplegado de chapa (STEP → DXF)

Convierte piezas de chapa 3D (STEP de cualquier CAD) en su **patrón plano** para corte.

### 5.1 Kernel geométrico
Usa **OpenCASCADE** (vía la librería `cadquery-ocp`) **solo como lector de geometría B-rep**
del STEP. La lógica de desplegado es propia (no depende de SolidWorks ni FreeCAD).

### 5.2 Algoritmo
1. **Lectura** de los sólidos del STEP.
2. **Clasificación de caras:** planos (paredes) y cilindros (dobleces).
3. **Detección de espesor:** separación modal entre pares de caras planas antiparalelas.
4. **Agrupación:** *paredes* (par exterior/interior) y *dobleces* (par de cilindros de radios
   `r` y `r + espesor` sobre el mismo eje).
5. **Grafo** pared–doblez–pared por aristas/tangencia compartida.
6. **Desplegado (BFS):** se fija una pared base en el plano XY y, por cada doblez, se coloca la
   pared vecina coplanar dejando la **longitud desarrollada neutra**
   `L = ángulo · (radio_interno + K · espesor)` (con **K-factor** configurable, por defecto 0,44).
7. **Unión** (Shapely) de las paredes desplegadas + rectángulos de doblez → contorno limpio con
   agujeros. Exporta DXF con el **corte** y las **líneas de doblez** en capas separadas.

### 5.3 Verificación dimensional
Para cada pieza se valida que `área_del_plano × espesor ≈ volumen del sólido` (control físico
independiente). Si el desvío supera la tolerancia (~5%), la pieza se marca como *REVISAR*. Esto
detecta automáticamente piezas que no son chapa de plegadora (tubos, perfiles, etc.).

### 5.4 Alcance y limitaciones
Soporta chapa de plegadora estándar: paredes planas + dobleces cilíndricos, espesor constante,
dobleces en árbol. No cubre: piezas cerradas con costura (tubos/cajas), superficies cónicas o
roladas, ni espesores variables.

---

## 6. Licencia / "modo pago"

- Un **flag global** (`settings/app_config.requireLicense` en Firestore) se controla desde el
  panel de administración.
- Al iniciar, el programa lee ese flag (vía Firestore REST). Si está activo, exige **vincular la
  cuenta**: muestra un **código de dispositivo**, abre el portal web para iniciar sesión y detecta
  la vinculación (`device_logins`). Valida que la cuenta tenga una **licencia activa**.
- Incluye un **período de prueba** local.
- La licencia validada se guarda localmente (`license.key` en *AppData*).

---

## 7. Diagnóstico de DXF

Para soporte y mejora, el programa puede **enviar los DXF cargados** al backend:

- **Cliente:** tras cargar un archivo, se sube de forma asíncrona (POST a
  `https://www.foglesting.com/api/upload-diagnostic`) el contenido (base64) + metadatos
  (nombre, tamaño, dimensiones de la pieza, equipo/usuario, versión). Protegido por una clave.
- **Servidor:** la función serverless valida la clave y guarda el documento en la colección
  `dxf_diagnostics` de Firestore (con tope de tamaño).
- **Admin:** la sección *Diagnósticos DXF* lista y permite **descargar** cada archivo para
  reproducir y corregir problemas.

> Esta recopilación está descrita en la [Política de Privacidad](../privacidad.html) y se anuncia
> en la página de descarga y dentro del programa.

---

## 8. Seguridad y privacidad (resumen)

- Toda la comunicación es por **HTTPS/TLS**.
- El panel de administración exige **autenticación** (Firebase) y **rol de administrador**
  (verificado en el servidor con `requireAdminUser`).
- Los endpoints sensibles validan token de admin o clave compartida según el caso.
- Minimización: no se recopilan datos de pago ni credenciales bancarias en el software.
- Detalle completo en la Política de Privacidad.

---

## 9. Compilación (build)

### Escritorio (GUI / CLI) — Windows + MinGW-w64
```bash
cmake -S . -B build_native -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++ -DCMAKE_RC_COMPILER=windres
cmake --build build_native --target sheet_nest_gui
```
La GUI se enlaza de forma **estática** (`-static`), por lo que el `.exe` no depende de DLLs del
runtime de MinGW.

### Web (WASM) — Emscripten
Target `nesting_wasm` (Emscripten), exporta el módulo `createNestingModule` consumido por
`online/`.

### Conversor STEP→DXF — Python
Requiere `cadquery-ocp` (OpenCASCADE), `shapely`, `ezdxf`, `numpy`.
```bash
python step2dxf/unfold.py pieza.step carpeta_salida
```

---

## 10. Limitaciones conocidas

- El desplegado de chapa cubre geometría de plegadora estándar (ver §5.4).
- La función "Agregar STEP" en el escritorio requiere el intérprete Python y el módulo de
  desplegado provistos junto al programa.
- El nesting es heurístico: más iteraciones/población mejoran el resultado a costa de tiempo.

---

*Foglesting — Powered by Foglar.*
