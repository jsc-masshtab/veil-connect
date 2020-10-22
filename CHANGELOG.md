
n.n.n / 2020-10-22
==================

  * FIX TG-10250 Removed  freerdp_handle_signals
  * FIX TG-10250 Small gui update
  * FIX TG-10250 Some TODOs resolved
  * FIX TG-10250 Refactoring
  * FIX TG-10250 Кнопка настройки
  * Revert "Merge branch 'feature_tg_10124' into master"
  * Merge branch 'feature_tg_10124' into master
  * FEATURE TG-10124 смена пользователя для WORKSPACE

1.3.3 / 2020-10-15
==================

  * Merge feature_tg_10051 into master
  * Merge branch 'feature_tg_10154' into 'master'
  * FEATURE TG-10154 Builds for ubuntu20, debian9, astra
  * Merge feature_tg_10050 into master
  * Update README.MD

1.3.2 / 2020-10-02
==================

  * FEATURE_TG_9923 patch version incremented
  * FEATURE_TG_9923 Cross compilation fix
  * FEATURE_TG_9923 fix warnings
  * FEATURE_TG_9923 Grab keyboard only in focus. Stable for 1 monitor on Linux
  * FEATURE_TG_9923 Grab keyboard only in focus. Prototype
  * FEATURE_TG_9923 Start implementing adaptive keyboard grab
  * FEATURE_TG_9923  RDP shared clipboard. Vm -> Client. Windows version prototype. Fix
  * FEATURE_TG_9923  RDP shared clipboard. Vm -> Client. Windows version prototype
  * FEATURE_TG_9923 RDP shared clipboard. Client -> Vm. Logic fix
  * FEATURE_TG_9923 RDP shared clipboard. Client -> Vm. Additional checks
  * FEATURE_TG_9923 RDP shared clipboard. Client -> Vm. Prototype
  * FEATURE_TG_9923 RDP shared clipboard. Client -> Vm implementation works. But Vm -> Client broken
  * FEATURE_TG_9923 RDP shared clipboard. Client -> Vm implementation 3
  * FEATURE_TG_9923 RDP shared clipboard. Client -> Vm implementation 2
  * FEATURE_TG_9923 RDP shared clipboard. Client -> Vm implementation
  * FEATURE_TG_9923 RDP shared clipboard. Works from vm to client
  * FEATURE_TG_9923 RDP shared clipboard Prototype
  * FEATURE_TG_9923 RDP shared clipboard
  * clipboard Наброски 2
  * clipboard Наброски
  * FEATURE TG-9803 Added missing headers
  * FEATURE TG-9803 Refactor vdi_ws_client
  * Merge branch 'master' of http://gitlab.bazalt.team/vdi/veil-connect
  * FEATURE TG-9803 Fix libusb_set_option
  * Add libusb-devel and usbredir-devel deps
  * FEATURE TG-9803  rdp arg fix
  * FEATURE TG-9803 Supposed bad rdp arg fix
  * Merge feature_tg_9858 into master

1.3.1 / 2020-09-21
==================

  * Merge branch 'feature_tg_9803
  * FEATURE TG-9803 patch version++
  * FEATURE TG-9803 Попытка поддержки remoteapp на freerdp 2.0.0
  * FEATURE TG-9803 Отображаем ошибку запуска приложения на гуи
  * FEATURE TG-9803 В gui можно указать название и опции запускаемого приложения
  * Update README.MD
  * Add gspawn-helper
  * FEATURE TG-9803 fix
  * FEATURE TG-9803 Завершаем RDP сессию если закрыты все окна в режиме одного приложения
  * FEATURE TG-9803 Start RDp rail implementing 2
  * FEATURE TG-9803 Start RDp rail implementing
  * FEATURE TG-9803 Наброски кода для запуска сессии rdp в режиме доступа только одного приложения
  * FEATURE TG-9803 Рефактор

1.3.0 / 2020-09-17
==================

  * FEATURE TG-9801 Small refactor
  *  FEATURE TG-9801 Linking fix
  * FEATURE TG-9801 Автоопределение адреса тк Fix
  * FEATURE TG-9801 Автоопределение адреса тк
  * FEATURE TG-9801 Платформозависимы код в __linux__
  * FEATURE TG-9801 Определение ip тк на линуксе (для usb tcp)
  * FEATURE TG-9801 gui improvement
  * Merge branch 'feature_tg_9641' into 'master'
  * Feature tg 9641
  * FEATURE TG-9662 Change Microsoft Visual C++ version in message box

1.2.19 / 2020-08-28
===================

  * FIX TG-9620 Фикс задания высоты картинки при подключении по RDP
  * Merge branch 'master' of http://gitlab.bazalt.team/vdi/veil-connect
  * Fixed leak in vdi_ws_client.c

1.2.18 / 2020-08-25
===================

  * Add openh264-6.dll to windows client
  * Merge master to master
  * small windows_rdp_launcher.c fix
  * Merge branch 'feature_tg_9531' into 'master'
  * FEATURE TG-9531 Update linux installer

1.2.17 / 2020-08-20
===================

  * FEATURE_TG_9534 При дисконекте по инициативе юзера (LOGOFF_BY_USER) закрываем rdp окна
  * FEATURE_TG_9543 RDP конфиг для нативного клиента создается в appdata (где у нас есть права на запись)
  * Merge branch 'featute_tg_9543'
  * FEATURE_TG_9543 Пишутся логи если путь содержит русские буквы
  * Fix pipeline
  * Введенf cmake опция BUILD_x32. (По дефолту OFF)
  * Merge branch 'feature_tg_9542' into 'master'
  * FEATURE TG-9542 Update FreeRDP version to 2.2.0
  * Merge branch 'feature_tg_9532' into 'master'
  * FEATURE TG-9532 Add build for 32-bit windows
  * Merge branch 'feature_tg_9532' into 'master'
  * FEATURE TG-9532 Add build for 32-bit windows
  * FEATURE_TG_9516 updated CHANGELOG.md

1.2.16 / 2020-08-17
===================

  * FEATURE_TG_9516 version patch increment
  * FEATURE_TG_9516 Если не установлен USBdk то показываем сообщение об этом при попытке проброса USB
  * FEATURE_TG_9514 Проброс принтеров

1.2.15 / 2020-08-11
===================

  * FEATURE TG-9428 Added forgotten header
  * FEATURE TG-9428 Supposed logging fix
  * FEATURE TG-9318 Убраны пароли из логирования
  * Added changelog

1.2.14 / 2020-08-10
===================

  * Merge branch 'feature_tg_9400' into 'master'
  * FEATURE TG-9400 Refactor linux installer
  * FEATURE_TG_9385 Параметр мультимониторности задается в GUI
  * FEATURE_TG_9385 Мультимониторность по ini параметру
  * FEATURE_TG_9385  image paint simplification
  * FEATURE_TG_9385 Кнопка выйти из фулскина на всех оенах (когда их больше 1)
  * FEATURE_TG_9301 fix archiving
  * FEATURE_TG_9301 Проверяем существование и права на папку при передача в freerdp
  * Merge into master
  * FEATURE_TG_9301 Проброс папок и дисков на виндовом клиенте
  * Update README.MD
  * FEATURE_TG_9301 patch  version increment
  * FEATURE_TG_9301 fix on windows
  * FEATURE_TG_9301 Кнопка архивация логов
  * FEATURE_TG-9300 Задание через GUI папок для проброса по RDP
  * FEATURE_TG-9246 Client-Type to User-Agent
  * FEATURE_TG-9246 Вывод на гуи сообщения ошибки  присылаемомго сервером при неудачном получении вм
  * Merge branch 'feature_tg_9241'
  * FEATURE TG-9241 small log fix
  *  FEATURE TG-9241 Log file name fix
  * FEATURE TG-9241 Переход на систему логирования glib
  * Fix symlink to latest version
  * FEATURE TG-9241 Readable log name format
  * patch version incremented.
  * Merge branch 'master' of http://gitlab.bazalt.team/vdi/veil-connect
  * FEATURE TG-8970 Подробнее сообщения об ошибках rdp
  * Merge branch 'feature_tg_9006' into 'master'
  * FEATURE TG-9006 Update build docs
  * Update vdi-connect.groovy
  * Merge branch 'feature_tg_9006' into 'master'
  * FEATURE TG-9006 Add build docs
  * FEATURE TG-8970 fix
  * FEATURE TG-8970 some refactor
  * Merge branch 'feature_tg_8970'
  * Merge branch 'master' of http://gitlab.bazalt.team/vdi/veil-connect
  * FEATURE TG-8970 Fix jsonhandler_get_data_or_errors_object
  * FEATURE TG-8970 фикс парсинга ответов сервера
  * FEATURE TG-8970 added gui messages
  * Rocketchat notify
  * FIX TG-8911 Выкинут нинужный код оставшийся о вирт вьювер
  * FIX TG-8911 ini file in LOCALAPPDATA. fixes
  * FIX TG-8911 ini file in LOCALAPPDATA

1.2.11 / 2020-06-30
===================

  * FEATURE TG-8910 Добавлено слово api во все урлы запросов как того хочет nginx
  * Merge branch 'feature_tg_8874' into 'master'
  * Feature tg 8874
  * FEATURE TG-8910 Removed outdated files
  * FEATURE TG-8910 limit message length
  * FIX TG-8910 Possible spice port parse fix
  * Merge branch 'feature_tg_8747' into 'master'
  * FEATURE-TG-8747 Add symlink to latest version
  * FEATURE-TG-8747 Rename deploy dir
  * Merge branch 'feature_tg_8713' into 'master'
  * FEATURE TG-8713 Add universal linux installer
  * Add postinstall script-4
  * Add postinstall script-3
  * Add postinstall script-2
  * Add postinstall script
  * Merge branch 'master' of http://gitlab.bazalt.team/vdi/veil-connect
  * FEATURE TG-8705 Incremented patch version
  * Update README.MD
  * Rename app file
  * Merge branch 'feature_tg_8680' into 'master'
  * FEATURE TG-8680 Add Centos build
  * Merge branch 'master' of http://gitlab.bazalt.team/vdi/veil-connect
  * FEATURE TG-8705 Changed binary file name to veil_connect
  * Update README.MD
  * FEATURE TG-8705 Added spice settings page
  * FEATURE TG-8705 Added toggle button for client cursor visibility control
  * FEATURE_TG_8611 Added ini param for image format
  * Merge branch 'master' of http://gitlab.bazalt.team/vdi/veil-connect
  * Increased max height for rdp image
  * Update README.MD
  * Update README.txt
  * Update README.txt
  * Merge
  * FIX TG-8531 Rdp arguments in ini
  * FEATURE TG-8605 Add desktop icon-3
  * FEATURE TG-8605 Add desktop icon-2
  * FEATURE TG-8605 Add desktop icon
  * Change install dir

1.2.9 / 2020-06-01
==================

  * added gfx-h264 parameter
  * FIX TG-8531 gui highlight small fix
  * Merge branch 'feature_tg_8442' into 'master'
  * FEATURE TG-8442 spice usb dk check
  * FEATURE TG-8442 Add dependencies check (win)

1.2.8 / 2020-05-31
==================

  * Version 1.2.8
  * FIX TG-8501 In settings  Highlight gui fields with incorect values
  * FIX TG-8501 Settings checking implementalion
  * FIX TG-8501 default port is 443
  * FIX TG-8170 https support
  * FIX TG-8498 stdoutput redirected to log file in release mode
  * FIX TG-8500 vdi indicator hint
  * Merge branch 'master' of http://192.168.10.145/vdi/veil-connect
  * FIX TG-8497 Removed default gui values
  * Version 1.2.7
  * MAX_MONITOR_AMOUNT 3
  * FIX TG-8410 Fixed unnamed usb devices (on win)
  * Fix publisher info
  * Add build-version for windows installer
  * Fix publisher info
  * Add usb.ids
  * translations folder fix
  * Merge branch 'feature_tg_8135' into 'master'
  * FEATURE TG-8135 Add pipeline for linux version-2
  * FEATURE TG-8135 Add pipeline for linux version
  * FEATURE TG-8121 Patch version incremented
  * FEATURE TG-8121 small refactor
  * FEATURE TG-8121 conn type choice implementing
  * FEATURE TG-8121 start conn type choice implementing
  * FEATURE TG-8121 Return server indicators
  * Chane windows name to Veil Connect
  * Change deploy path-2
  * Change deploy path
  * Fix gitlab repo name
  * Merge branch 'feature_tg_7975' into 'master'
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-10
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-9
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-8
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-7
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-6
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-5
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-4
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-3
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative-2
  * FEATURE TG-7975 Change jenkins pipeline syntax to declarative
  * FEATURE TG-7975 Fix installer ico-2
  * FEATURE TG-7975 Fix installer ico
  * FEATURE TG-7975 Add jenkins pipeline for veil connect
  * Initial commit
  * Add new file
