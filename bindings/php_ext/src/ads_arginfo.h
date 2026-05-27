/* ads_arginfo.h — arginfo for all PHP_ME entries in the openads extension.
 * Included by each .c file that registers methods.
 * All entries are static so multiple-inclusion is safe.
 */
#ifndef ADS_ARGINFO_H
#define ADS_ARGINFO_H

/* -----------------------------------------------------------------------
 * AdsConnection
 * --------------------------------------------------------------------- */

/* connect(array $options): static */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_connection_connect, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

/* close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_connection_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* query(string $sql): AdsStatement */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_connection_query, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* execute(string $sql): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_connection_execute, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* beginTransaction(): AdsTransaction */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_connection_begin_transaction, 0, 0, 0)
ZEND_END_ARG_INFO()

/* isAlive(): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_connection_is_alive, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* prepare(string $sql): AdsPreparedStatement */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_connection_prepare, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* -----------------------------------------------------------------------
 * AdsStatement
 * --------------------------------------------------------------------- */

/* fetchAssoc(): array|false */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_statement_fetch_assoc, 0, 0, 0)
ZEND_END_ARG_INFO()

/* fetchRow(): array|false */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_statement_fetch_row, 0, 0, 0)
ZEND_END_ARG_INFO()

/* fetchAll(): array */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_statement_fetch_all, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

/* rowCount(): int */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_statement_row_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* columnCount(): int */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_statement_column_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_statement_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* -----------------------------------------------------------------------
 * AdsTable
 * --------------------------------------------------------------------- */

/* open(AdsConnection $conn, string $tablePath,
 *      int $tableType=ADS_ADT, int $lockType=ADS_COMPATIBLE_LOCKING,
 *      int $charType=ADS_ANSI, int $openMode=ADS_SHARED): static */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_table_open, 0, 0, 2)
    ZEND_ARG_OBJ_INFO(0, conn, AdsConnection, 0)
    ZEND_ARG_TYPE_INFO(0, tablePath, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, tableType, IS_LONG, 0, "3")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, lockType,  IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, charType,  IS_LONG, 0, "1")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, openMode,  IS_LONG, 0, "4")
ZEND_END_ARG_INFO()

/* close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* gotoTop(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_goto_top, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* gotoBottom(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_goto_bottom, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* skip(int $n = 1): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_skip, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, n, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()

/* gotoRecord(int $recNo): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_goto_record, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, recNo, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* atEOF(): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_at_eof, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* atBOF(): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_at_bof, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* getRecord(): array */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_get_record, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

/* getString(string $field): string */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_get_string, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* getDouble(string $field): float */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_get_double, 0, 1, IS_DOUBLE, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* getLong(string $field): int */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_get_long, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* getLogical(string $field): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_get_logical, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* setString(string $field, string $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_set_string, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* setLong(string $field, int $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_set_long, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* setDouble(string $field, float $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_set_double, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

/* setLogical(string $field, bool $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_set_logical, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, field, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* appendRecord(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_append_record, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* deleteRecord(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_delete_record, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* writeRecord(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_write_record, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* cancelUpdate(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_cancel_update, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* recordCount(): int */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_record_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* recordNum(): int */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_table_record_num, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* -----------------------------------------------------------------------
 * AdsTransaction
 * --------------------------------------------------------------------- */

/* commit(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_transaction_commit, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* rollback(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_transaction_rollback, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* isActive(): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_transaction_is_active, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* -----------------------------------------------------------------------
 * AdsDictionary
 * --------------------------------------------------------------------- */

/* open(string $dictPath, string $user="", string $pass=""): static */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_dictionary_open, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, dictPath, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, user, IS_STRING, 0, "\"\"")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, pass, IS_STRING, 0, "\"\"")
ZEND_END_ARG_INFO()

/* fromConnection(AdsConnection $conn): static */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_dictionary_from_connection, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, conn, AdsConnection, 0)
ZEND_END_ARG_INFO()

/* close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_dictionary_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* getTableProperty(string $tableName, int $property): string */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_dictionary_get_table_property, 0, 2, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, tableName, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, property,  IS_LONG,   0)
ZEND_END_ARG_INFO()

/* getDatabaseProperty(int $property): string */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_dictionary_get_database_property, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, property, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* -----------------------------------------------------------------------
 * AdsPreparedStatement
 * --------------------------------------------------------------------- */

/* bindString(string $name, string $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_string, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* bindInt(string $name, int $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_int, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_LONG,   0)
ZEND_END_ARG_INFO()

/* bindDouble(string $name, float $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_double, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

/* bindBool(string $name, bool $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_bool, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, _IS_BOOL,  0)
ZEND_END_ARG_INFO()

/* bindDate(string $name, string $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_date, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* bindTimestamp(string $name, string $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_timestamp, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* bindMoney(string $name, int $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_money, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_LONG,   0)
ZEND_END_ARG_INFO()

/* bindBinary(string $name, string $data, int $type = ADS_BINARY): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_binary, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, type, IS_LONG, 0, "6")
ZEND_END_ARG_INFO()

/* bindNull(string $name): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind_null, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* bind(string $name, mixed $value): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_bind, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, name,  IS_STRING, 0)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

/* execute(): AdsStatement|true */
ZEND_BEGIN_ARG_INFO_EX(arginfo_ads_prepared_execute, 0, 0, 0)
ZEND_END_ARG_INFO()

/* paramCount(): int */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_param_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* close(): void */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ads_prepared_close, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

#endif /* ADS_ARGINFO_H */
