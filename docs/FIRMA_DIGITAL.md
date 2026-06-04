# Firma digital del ejecutable (code signing)

> Guía para firmar `foglesting.exe` con un certificado de firma de código, de modo que
> Windows muestre el **editor verificado** y se reduzca/elimine la advertencia de
> **SmartScreen / "Editor desconocido"**.

---

## 1. Por qué firmar

Cuando un usuario descarga y abre un `.exe` **sin firmar**, Windows muestra:

- *"Windows protegió tu PC" (SmartScreen)* y/o
- *"Editor: Desconocido"* en el cuadro de Control de cuentas de usuario (UAC).

Firmar el ejecutable con un certificado emitido por una **Autoridad Certificante (CA)**:

- Muestra el **nombre verificado de la empresa** como editor.
- Garantiza la **integridad** del archivo (no fue alterado tras compilar).
- Mejora la **reputación** en SmartScreen (con EV es inmediata; con OV se construye con el tiempo).

Para un cliente de producción, este es el ítem de mayor impacto en confianza.

---

## 2. Qué certificado comprar

| Tipo | Verificación | SmartScreen | Costo aprox. (anual) |
|---|---|---|---|
| **OV** (Organization Validation) | Verifica la existencia de la empresa | Construye reputación con el tiempo | USD ~100–250 |
| **EV** (Extended Validation) | Verificación reforzada de la empresa | **Reputación inmediata** (sin período de "calentamiento") | USD ~250–450 |

**Emisores recomendados:** Sectigo (ex Comodo), DigiCert, GlobalSign, SSL.com.

> ⚠️ **Importante (CA/Browser Forum, vigente desde 2023):** las claves privadas de los
> certificados OV/EV **deben residir en hardware** (token USB FIPS 140-2 o HSM en la nube).
> Ya **no** se entregan como archivo `.pfx` descargable. Opciones:
> - El emisor te envía un **token USB** con la clave, **o**
> - Usás un **servicio de firma en la nube** (cloud signing / HSM) del emisor.
>
> Para Argentina, comprar a un emisor internacional (Sectigo/DigiCert) es lo habitual; la
> verificación de la empresa puede pedir documentación (constancia de inscripción, teléfono
> verificable, etc.).

---

## 3. Herramienta: `signtool`

`signtool.exe` viene con el **Windows SDK**. Si no lo tenés:

- Instalá el *Windows 10/11 SDK* (o las *Build Tools*). Queda en, por ejemplo:
  `C:\Program Files (x86)\Windows Kits\10\bin\<versión>\x64\signtool.exe`.

---

## 4. Firmar el ejecutable

### Caso A — Certificado en token USB / instalado en el almacén de Windows
Con el token conectado (o el certificado importado en el almacén "Personal"):

```bat
signtool sign ^
  /n "Razon Social S.A." ^
  /fd sha256 ^
  /tr http://timestamp.sectigo.com ^
  /td sha256 ^
  /v ^
  foglesting.exe
```

- `/n "Razon Social S.A."` → nombre del sujeto del certificado (Common Name de la empresa).
  Alternativamente `/sha1 <thumbprint>` para elegir por huella.
- `/fd sha256` → algoritmo de hash del archivo.
- `/tr ... /td sha256` → **sellado de tiempo (timestamp)**. Imprescindible: hace que la firma
  siga siendo válida **después** de que el certificado expire.
- `/v` → salida detallada.

Al firmar con un token de hardware, se te pedirá el **PIN** del token.

### Caso B — Firma en la nube (cloud HSM del emisor)
Los emisores con firma en la nube proveen su propio plugin/cliente (p. ej. DigiCert KeyLocker,
SSL.com eSigner). El comando es similar pero usando el proveedor de la nube en lugar de `/n`.
Seguí la documentación del emisor (suele integrarse con `signtool` mediante una librería KSP/CSP).

---

## 5. Verificar la firma

```bat
signtool verify /pa /v foglesting.exe
```

Debe indicar **"Successfully verified"** y mostrar el editor y la marca de tiempo. También podés
verlo en *clic derecho → Propiedades → Firmas digitales*.

---

## 6. Integración en el flujo de release

1. Compilar el `.exe` (ver `DOCUMENTACION_TECNICA.md` §9).
2. **Firmar** el `.exe` con `signtool` (paso 4).
3. **Verificar** (paso 5).
4. Subir el `.exe` firmado al panel de administración (carpeta `admin-releases/`) y publicarlo
   cuando esté probado.

> Recomendación: firmar **siempre** la versión que se publica al público. Las builds internas de
> prueba pueden ir sin firmar, pero conviene firmar también las que se compartan con clientes.

---

## 7. Notas

- **No** publiques ni subas al repositorio la clave privada, el `.pfx` (si lo hubiera), ni el PIN
  del token.
- El **timestamp** evita que las firmas "caduquen": un binario firmado y sellado sigue siendo
  válido aunque el certificado expire después.
- Si cambiás de certificado, volvé a firmar las versiones que quieras mantener verificadas.

---

*Foglesting — Powered by Foglar.*
