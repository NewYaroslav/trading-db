# trading-db

Данная библиотека является набором удобных оболочек для sqlite, которые можно использовать в алготрейдинге

На данный момент есть следующие классы БД:

* BetDatabase в файле bet-database.hpp 				- БД для хранения ставок фиксированных контрактов (бинарных опционов)
* KeyValueDatabase в файле key-value-database.hpp 	- БД для хранения строковых пар ключ-значение
* ListDatabase в файле list-database.hpp 			- БД для хранения уникальных или повторяющихся строковых значений
* IntKeyBlobValueDatabase в файле int-key-blob-value-database.hpp - Шаблонный вспомогательный класс БД для хранения бинарных данных по целому ключу

Примеры можно найти в code-blocks/examples

