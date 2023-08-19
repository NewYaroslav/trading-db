# trading-db

Данная библиотека является набором удобных оболочек для sqlite, которые можно использовать в алготрейдинге

На данный момент есть следующие классы БД:

* BetDatabase в файле bet-database.hpp 				- БД для хранения ставок фиксированных контрактов (бинарных опционов)
* KeyValueDatabase в файле key-value-database.hpp 	- БД для хранения строковых пар ключ-значение
* ListDatabase в файле list-database.hpp 			- БД для хранения уникальных или повторяющихся строковых значений
* IntKeyBlobValueDatabase в файле int-key-blob-value-database.hpp - Шаблонный вспомогательный класс БД для хранения бинарных данных по целому ключу
* QDB в файле qdb.hpp								- БД для хранения тиков и баров в сжатом виде.

Примеры можно найти в code-blocks/examples и code-blocks/qdb

### Сборка библиотеки googletest под MinGW

В папке библиотеки lib/ googletest сделать следующее:

```
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make.exe
```
