# Metro Autotests Notes

## Что это за работа

В репозитории добавлен отдельный слой сценарных автотестов для OpenTTD, ориентированный на разработку метро и multilayer-инфраструктуры.

Идея такая:

1. OpenTTD запускается headless из `ctest`.
2. GameScript сам строит нужную схему в игровом мире.
3. GameScript не пишет "свободные" логи как источник истины, а в конце отдаёт один структурированный JSON-результат.
4. Раннер на стороне CMake ловит этот JSON, валидирует его и превращает в нормальный CI/dev-loop артефакт.
5. При необходимости сценарий ещё и сохраняет готовый `.sav`, чтобы мир можно было открыть вручную и глазами проверить схему.

Это позволяет использовать одну и ту же связку и для CI, и для локальной разработки, и для итераций с LLM.

## Что уже реализовано

### 1. Структурированный JSON-репорт из GameScript

Добавлен `ScriptTest`:

- `src/script/api/script_test.hpp`
- `src/script/api/script_test.cpp`

Что он умеет:

- `GSTest.Report(table data)`
  - сериализует squirrel-таблицу в JSON
  - валидирует минимальную схему отчёта
  - печатает строку вида `OTTD_TEST_RESULT <json>` в stderr
- `GSTest.SaveGame("name.sav")`
  - сохраняет текущий игровой мир в save directory
  - используется сценариями как финальный артефакт для ручной проверки

Ограничения `GSTest.SaveGame`:

- принимает только простой filename
- запрещает пути, `..`, `:` и разделители каталогов
- автоматически дописывает `.sav`, если расширение не указано

### 2. Новый сценарный CMake-слой

Добавлены/расширены:

- `cmake/CreateScenario.cmake`
- `cmake/scripts/Scenario.cmake`

Что делает инфраструктура:

- регистрирует `scenario_<suite>` как отдельный тест CTest
- копирует `info.nut`, `main.nut`, config и fixture-файлы в build-каталог
- запускает `openttd` headless
- ожидает ровно один `OTTD_TEST_RESULT`
- сохраняет результат в
  - `scenario_<suite>_result.json`
- сохраняет полный stderr в
  - `scenario_<suite>_output.txt`
- валит тест, если:
  - crash
  - timeout
  - нет отчёта
  - отчётов больше одного
  - JSON без обязательных полей
  - верхний `status` в репорте не равен `pass`

Отдельно добавлена поддержка `SAVE_ARTIFACT` в `create_scenario_test(...)`.

Если сценарий объявлен как:

```cmake
create_scenario_test(
    ...
    SAVE_ARTIFACT my_world.sav
    ...
)
```

то раннер после прогона ожидает, что сценарий реально сохранил этот файл в `scenario/save/`, и затем копирует его в build root как:

`scenario_<suite>_artifact.sav`

### 3. Поиск графического набора для headless-сценариев

В `cmake/scripts/Scenario.cmake` добавлен автопоиск usable OpenGFX.

Порядок поиска:

- `OPENTTD_BASESET_DIR`
- `${HOME}/.local/share/openttd/baseset`
- `${HOME}/Documents/OpenTTD/baseset`
- на Windows: `${PUBLIC}/Documents/OpenTTD/baseset`

После нахождения runner копирует набор в `${build}/baseset`.

Это позволяет запускать headless world-generation и loading даже для сценариев, где нужен полноценный игровой мир.

### 4. GS helper library для метро

Ключевая библиотека:

- `bin/game/library/metrotest/main.nut`

Что там есть:

- `MetroTestLib`
  - сборка `checks`
  - сборка `metrics`
  - финальный вызов `GSTest.Report(...)`
- `MetroCompanyLib`
  - создание/получение компании
  - выдача денег
  - выбор текущего rail type через `GSRailTypeList`
- `MetroBuildLib`
  - helpers для surface rail
  - station/depot helpers
  - tile helpers
- `MetroLib`
  - helpers для portal/underground/station build
- `MetroTrainLib`
  - поиск buildable локомотива
  - поиск buildable вагона
  - покупка поезда
  - прицепка вагона
  - добавление surface/underground orders
  - polling helpers для ожидания выхода из депо, захода под землю и посещения станций

### 5. GS-only metro scenario

Добавлен сценарий:

- `regression/scenarios/metro_loop_two_portals_dual_underground/`

Файлы:

- `CMakeLists.txt`
- `scenario.cfg`
- `info.nut`
- `main.nut`

Сценарий задуман так:

- на поверхности есть loop
- на верхней ветке есть surface station
- есть depot
- есть 2 портала
- под землёй есть 2 станции на разных участках трассы
- перед каждой underground station ставится signal
- поезд состоит из 1 локомотива и 1 вагона
- orders должны включать все станции
- после прохода surface station поезд должен снова прийти на первую underground station, что и доказывает замкнутость loop

## Как устроен конкретный сценарий `metro_loop_two_portals_dual_underground`

### Общая топология

Текущая схема сценария такая:

1. Поверхность:
   - верхняя ветка с `Metro Surface`
   - нижняя ветка
   - правый разворот, который замыкает surface-loop
   - два портала слева: верхний для выхода из подземки, нижний для входа в подземку
   - depot стоит на верхней ветке в валидной X-ориентации

2. Подземка:
   - нижняя подземная ветка от нижнего портала
   - `Metro Underground B`
   - левый нижний поворот
   - вертикальный подземный соединитель
   - левый верхний поворот
   - `Metro Underground A`
   - верхняя подземная ветка к верхнему порталу

3. Движение по loop задумано так:
   - поезд выходит из depot вправо по верхней ветке
   - проходит правый surface-разворот
   - идёт по нижней ветке к нижнему порталу
   - заходит под землю
   - проходит `Metro Underground B`
   - затем `Metro Underground A`
   - выходит через верхний портал на поверхность
   - проходит `Metro Surface`
   - снова уходит на следующий круг

Это именно loop-сценарий, а не линейный smoke-test: цель не просто построить пути, а воспроизвести полный циклический маршрут с surface + underground сегментами.

### Orders по станциям

Сейчас в сценарии поезду задаются обычные orders для всех станций:

1. underground station A
2. underground station B
3. surface station

Это делается через:

- `trains.append_underground_order(vehicle, station_a_id)`
- `trains.append_underground_order(vehicle, station_b_id)`
- `trains.append_surface_order(vehicle, surface_station_tile)`

Дополнительно сценарий проверяет:

- что каждый order был успешно добавлен
- что итоговый `GSOrder.GetOrderCount(vehicle)` равен `3`

Важно:

- orders сейчас намеренно оставлены "обычными", без подавления погрузки/разгрузки
- если на попытке обычной station-loading логика метро падает, это считается полезным reproducer-результатом для разработки

### Проверка реального обхода станций

В `main.nut` сценарий не ограничивается проверкой "ордеры записаны".

Он реально ждёт:

- выход из депо
- вход под землю через нижний портал
- первый визит на underground station A
- потом визит на underground station B
- потом визит на surface station
- потом второй визит на underground station A

Это значит, что сценарий проверяет уже не конфигурацию orders, а фактическое поведение поезда на построенной схеме.

## Что было найдено по пути

### 1. Баг в scenario runner

Раннер раньше ошибочно считал тест успешным, если в JSON просто встречалась подстрока `"status":"pass"`.

Проблема:

- в отчёте верхний `status` мог быть `fail`
- но внутри `checks` всё равно были отдельные успешные проверки со строкой `"status":"pass"`
- из-за этого раннер пропускал реально красные сценарии как зелёные

Это исправлено в `cmake/scripts/Scenario.cmake`.

Теперь runner смотрит именно на верхний статус отчёта.

### 2. Сохранение сейва было сделано, но сама схема ещё не доведена до конца

Сохранение `.sav` уже работает.

То есть после прогона сценария действительно можно открыть мир вручную.

Но в процессе ручной проверки стало видно, что именно геометрия loop в `metro_loop_two_portals_dual_underground` ещё требует доработки.

Ключевые причины, найденные в ходе отладки:

- попытка строить обычный track поверх station tiles
- терраформинг portal tile слишком поздно, когда соседние rails уже мешают менять slope
- попытка ставить underground signals на tiles, где ещё нет underground track slice
- неправильные corner-piece типы на некоторых поворотах
- различие между тем, как поезд реально проходит через `StationTile`, и тем, как некоторые вспомогательные проверки смотрят на `Track` only

### 3. Направление порталов зависит не от аргумента API, а от slope tile

`CmdBuildPortal(...)` в `src/multilayer/underground_cmd.cpp` фактически берёт направление из `GetInclinedSlopeDirection(slope)`.

Это важно:

- портал нельзя считать "произвольным объектом"
- его вход/выход привязаны к tile slope
- если slope задан не в ту сторону, поезд либо не войдёт в портал, либо выйдет не туда, куда ожидалось

### 4. Underground signal можно поставить только на `SliceKind::Track`

Если на глубине в этой плитке находится `StationTile`, то `BuildUndergroundSignal(...)` вернёт ошибку.

Это означает:

- signal должен стоять на отдельной track-плитке перед станцией
- нельзя пытаться ставить его прямо на station tile

### 5. Depot для верхней ветки должен быть в X-ориентации

В текущем loop-сценарии верхняя surface-ветка построена как `RAILTRACK_NE_SW`.

Из практической отладки сценария вышло важное правило:

- depot, подвешенный "сверху/снизу" к такой ветке, даёт неправильный выезд
- для этой схемы depot нужно ставить в X-ориентации прямо на верхней ветке
- после этого поезд корректно уходит из depot в loop и доезжает до нижнего портала

Именно поэтому в `metro_loop_two_portals_dual_underground/main.nut` depot больше не висит отдельным вертикальным аппендиксом под линией.

### 6. Текущее воспроизводимое падение происходит на первой подземной станции при обычной загрузке

После правки depot и surface-геометрии сценарий уже проходит дальше:

- поезд выходит из depot
- реально входит в подземку через нижний портал
- доезжает до `Metro Underground B`

На этом месте при обычных station orders наблюдается reproducible crash в `Vehicle::BeginLoading()`.

По факту это сейчас полезный regression/reproducer:

- сценарий строит корректную схему достаточно далеко, чтобы дойти до проблемного места
- падение происходит не "в случайном месте", а стабильно на попытке обслуживания первой подземной станции
- stacktrace и crash-summary уже дают разработчику точку входа в баг

Текущая позиция по этому сценарию такая:

- crash не заметается под ковёр через специальные order flags
- если OpenTTD падает и оставляет crash-данные, это считается ожидаемым и полезным результатом для разработки бага
- сценарий всё равно остаётся ценным, потому что воспроизводит проблему детерминированно

## Где искать результат и артефакты

### JSON-отчёт

Пример build-artifact:

- `scenario_metro_loop_two_portals_dual_underground_result.json`

### Полный stderr сценария

- `scenario_metro_loop_two_portals_dual_underground_output.txt`

### Savegame, который можно открыть вручную

Runner сохраняет его как:

- `scenario_metro_loop_two_portals_dual_underground_artifact.sav`

Дополнительно я копировал сейв в Windows-удобное место:

- `E:\open-ttd\OpenTTD\artifacts\saves\metro_loop_two_portals_dual_underground_artifact.sav`

### Crash-данные

Если сценарий падает, основным артефактом становятся:

- crash-summary и stacktrace прямо в выводе `ctest`
- `scenario_metro_loop_two_portals_dual_underground_output.txt`
- `scenario_metro_loop_two_portals_dual_underground_result.json`, если сценарий успел отдать JSON до падения

Практический нюанс:

- если crash случился до финального `GSTest.SaveGame(...)`, последний `artifact.sav` может быть сохранением от предыдущего прогона, а не от текущего crash-run
- для анализа именно падения в первую очередь нужно смотреть stacktrace и stderr runner-а

## Пользовательский fixture-save

Для `metro_loop_two_portals_dual_underground` теперь отдельно используется пользовательский save с ручной правкой выезда из depot.

Файл fixture:

- `regression/scenarios/metro_loop_two_portals_dual_underground/depot_exit_fixed.sav`

Источник этого fixture:

- пользователь вручную поправил выезд из depot
- затем отдельно запустил поезд
- затем снова пересохранил мир

Этот save полезен не как "идеальный финальный layout", а как детерминированный reproducer уже на runtime-состоянии мира.

### Что сейчас известно по этому fixture

1. Fixture копируется в scenario build-каталог и реально используется runner-ом.
2. Для последнего пользовательского save fixture был сверен по `md5`, чтобы исключить ситуацию, когда в тест попал не тот файл.
3. При запуске именно с этим save падение воспроизводится очень быстро.

Зафиксированное поведение:

- поезд уже запущен в самом save
- OpenTTD падает примерно за `1` секунду после старта прогона
- это укладывается в требование "должен упасть в течение 10 секунд"
- crash снова происходит в `Vehicle::BeginLoading()`

### Что отличается от обычного сценарного прогона

При запуске с этим пользовательским fixture:

- `game_script` в crash-summary сейчас `null`
- верхний `OTTD_TEST_RESULT` не успевает появиться
- значит падение происходит раньше, чем наш сценарный GS успевает отдать структурированный JSON

Практически это означает следующее:

- этот fixture сейчас хорош именно как crash-reproducer для движка
- он пока не подходит как обычный "зелёный" scenario fixture, который должен дожить до `GSTest.Report(...)`
- но для разработки бага он даже полезнее, потому что сокращает время до падения до примерно одной секунды

### Последнее воспроизведение

По последнему прогону с пользовательским save:

- старт прогона: `18:11:12`
- завершение crash-run: примерно через `1.01 sec`
- `savegame_size` в crash-summary: `24128`
- `game_script`: `null`
- стек снова упирается в `Vehicle::BeginLoading()`

То есть на текущем этапе это уже не "случайный" крэш, а очень короткий и стабильный reproducer для отладки загрузки на underground station.

## Как это запускать

### Только этот один сценарий

Из build-каталога:

```bash
ctest -R scenario_metro_loop_two_portals_dual_underground --output-on-failure
```

Если base set не найден автоматически:

```bash
OPENTTD_BASESET_DIR=/path/to/baseset ctest -R scenario_metro_loop_two_portals_dual_underground --output-on-failure
```

### Полный metro-suite

```bash
ctest -R scenario_metro_ --output-on-failure
```

## Текущее состояние на момент этого документа

Готово:

- JSON-feedback для сценариев
- GS API для `GSTest.Report`
- GS API для `GSTest.SaveGame`
- CMake/CTest runner для сценариев
- перенос `.sav` в artifact
- metro helper library
- сценарий `metro_loop_two_portals_dual_underground`
- orders по всем станциям
- runtime-проверка выхода из depot и захода в underground loop
- детерминированный runtime-reproducer падения на первой underground station при обычной загрузке
- отдельный пользовательский fixture-save с уже запущенным поездом
- воспроизведение падения с fixture примерно за одну секунду

Ещё в работе:

- доведение полного loop до честного `status = pass` после исправления engine-side бага загрузки на underground station
- дополнительная фиксация артефактов именно для crash-run, если понадобится отдельный save перед стартом поезда
- собственно engine-side fix в `Vehicle::BeginLoading()` / station-loading path для underground station runtime

То есть инфраструктура автотестов уже работает, сейвы уже сохраняются, а сам двухпортальный loop-сценарий уже дошёл до уровня reproducible bug test: он не просто строит мир, а стабильно довозит поезд до проблемного места.

## Какие файлы сейчас ключевые

Если нужно быстро понять систему, смотри в таком порядке:

1. `src/script/api/script_test.hpp`
2. `src/script/api/script_test.cpp`
3. `cmake/CreateScenario.cmake`
4. `cmake/scripts/Scenario.cmake`
5. `bin/game/library/metrotest/main.nut`
6. `regression/scenarios/metro_loop_two_portals_dual_underground/CMakeLists.txt`
7. `regression/scenarios/metro_loop_two_portals_dual_underground/main.nut`

## Короткий вывод

Сделан не просто "один тест", а полноценный сценарный каркас для разработки метро:

- GameScript строит мир
- test runner получает машиночитаемый JSON
- CI может валить тесты по реальному состоянию мира
- разработчик может открыть сохранённый `.sav` и визуально проверить, что именно построилось

Сейчас главный оставшийся шаг не инфраструктурный, а движковый: сценарий и пользовательский fixture уже дают быстрый и стабильный reproducer. После починки падения на `BeginLoading()` их можно будет вернуть из режима crash-test в полноценный runtime `pass` сценарий.
