# Empaquetado de Windows

Esta carpeta concentra todo lo específico de distribución para no mezclarlo
con el motor NEC2.

- `upstream.env`: procedencia inmutable consumida por scripts y CI.
- `launcher/`: fuente C, recursos de versión y manifiesto del launcher Win32.
- `build-launcher.sh`: compila un PE de subsistema GUI reproducible en UCRT64.
- `package-portable.sh`: reúne el ejecutable, DLL transitivas, módulos GTK,
  datos de runtime, manifiesto de paquetes y hashes.
- `xnec2c.cmd`: inicio alternativo con consola, reservado para diagnóstico.
- `xnec2c.iss`: definición del instalador por usuario de Inno Setup.
- `build-installer.ps1`: localiza `ISCC.exe`, valida el portable y compila el
  Setup.
- `msix/AppxManifest.xml.in`: plantilla del manifiesto MSIX para Windows
  Desktop (`runFullTrust`).
- `msix/StoreIdentity.json`: identidad oficial reservada en Microsoft Partner
  Center para Xnec2c (`CharlyGolf.Xnec2c`).
- `build-msix.ps1`: genera los recursos visuales, construye el manifiesto con
  la identidad de Store, empaqueta con `MakeAppx.exe` y vuelve a desempaquetar
  el MSIX como comprobación estructural.

El workflow de GitHub Actions genera tres distribuciones desde la misma
compilación: el portable, el instalador Inno Setup y el paquete MSIX para
Microsoft Store. De esta forma no se mantiene una segunda lista independiente
de DLL para cada formato.

La identidad oficial de Microsoft Store configurada actualmente es:

- Package/Identity/Name: `CharlyGolf.Xnec2c`
- Package/Identity/Publisher: `CN=22DE0707-9E03-4F93-9B58-1F1C7076D4F9`
- Package/Properties/PublisherDisplayName: `CharlyGolf`
- Package Family Name: `CharlyGolf.Xnec2c_ybtc6319ae5ha`
- Store ID: `9PBVK60LXV1P`

El instalador consume la salida de `package-portable.sh`; no mantiene una
segunda lista independiente de DLL. La asociación `.nec` está incluida detrás
de `EnableNecAssociation=0` y no se compila en esta etapa.

Los scripts sólo reemplazan automáticamente destinos de compilación previstos
bajo `dist/` y directorios temporales de CI. Esta restricción evita borrar por
accidente un directorio arbitrario cuando se introduce una ruta incorrecta.
