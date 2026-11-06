# wkRandMap 0.1.0

Módulo WormKit experimental para Worms Armageddon 3.8.1 de 32 bits.

## Instalación

1. Cierra Worms Armageddon.
2. Copia `wkRandMap.dll` y `wkRandMap.ini` junto a `WA.exe`.
3. Activa **Options → Advanced → Load WormKit modules** y reinicia el juego.
4. Crea la carpeta `User\SavedLevels\RandMap` y agrega mapas que ya puedas abrir manualmente desde el editor del juego.

Para usar otra carpeta, edita el INI. Puedes usar una ruta relativa a `WA.exe` o una ruta absoluta.

```ini
[RandMap]
Folder=D:\Worms\Mapas WXW
```

El módulo lee el INI cada vez que ejecutas el comando. Si la ruta contiene tildes u otros caracteres fuera de la página de códigos del sistema, guarda el INI como UTF-16 LE con BOM.

## Uso y prueba

1. Crea un lobby como host, por ejemplo en LAN.
2. Escribe `/randmap` en el chat y presiona Enter. El editor puede aparecer brevemente mientras se importa el mapa. El chat local indica el nombre elegido o un error.
3. Comprueba que cambie la vista previa y, con un segundo cliente, verifica que reciba el mapa y pueda iniciar la partida.

El sorteo incluye archivos `.png`, `.bit`, `.lev` e `.img` directamente en la carpeta configurada. Cada sorteo es independiente y puede volver a elegir el mismo mapa. El juego valida el contenido al importarlo. Solo el host instala el módulo. El envío del mapa usa la lógica normal del lobby.

La carga crea una copia temporal dentro de `User\SavedLevels`, por lo que el juego necesita permiso de escritura ahí. Para desinstalar, cierra el juego y retira la DLL y el INI.

## Compatibilidad y compilación

DLL PE32 compilada y puntos de entrada cotejados con el ejecutable del parche oficial 3.8.1 para CD. La DLL comprueba las firmas de las rutinas y se desactiva cuando difieren. Falta probarla dentro de una partida en Windows, incluidas las ediciones Steam y GOG. La detección de versión incompatible se registra mediante `OutputDebugStringA`.

Para compilar con Zig 0.15.2 desde la carpeta que contiene el código:

```sh
zig cc -target x86-windows-gnu -O2 -Wall -Wextra -Werror -shared -o wkRandMap.dll wkRandMap.c -lbcrypt
```

Referencias técnicas: ejecutable del parche 3.8.1 distribuido por [Worms Knowledge Base](https://worms2d.info/Updates_(Worms_Armageddon)) y convenciones del chat documentadas en el código de [wkTerrainSync](https://github.com/rm-rf-worms/wkTerrainSync).
