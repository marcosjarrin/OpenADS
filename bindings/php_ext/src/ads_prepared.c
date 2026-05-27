#include "php_ads.h"
#include "ads_arginfo.h"

zend_object_handlers ads_prepared_handlers;

/* -----------------------------------------------------------------------
 * Object lifecycle
 * --------------------------------------------------------------------- */
static zend_object *ads_prepared_create_object(zend_class_entry *ce)
{
    ads_prepared_obj *obj = (ads_prepared_obj *)
        zend_object_alloc(sizeof(ads_prepared_obj), ce);
    obj->hStmt  = 0;
    obj->closed = 1;
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &ads_prepared_handlers;
    return &obj->std;
}

static void ads_prepared_free_object(zend_object *obj)
{
    ads_prepared_obj *intern = ads_prep_from_obj(obj);
    if (!intern->closed && intern->hStmt != 0) {
        AdsCloseSQLStatement(intern->hStmt);
        intern->hStmt  = 0;
        intern->closed = 1;
    }
    zend_object_std_dtor(obj);
}

/* -----------------------------------------------------------------------
 * Internal helper — strip leading ':' if present.
 * ACE AdsSet* functions for prepared statements expect the bare name
 * (e.g. "val" or "1"), not the SQL marker syntax (":val").
 * --------------------------------------------------------------------- */
static const char *prep_param_name(const char *name, char *buf, size_t bufsz)
{
    if (name[0] == ':') {
        snprintf(buf, bufsz, "%s", name + 1);
        return buf;
    }
    return name;
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::close() : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, close)
{
    ZEND_PARSE_PARAMETERS_NONE();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    if (!obj->closed && obj->hStmt != 0) {
        AdsCloseSQLStatement(obj->hStmt);
        obj->hStmt  = 0;
        obj->closed = 1;
    }
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::paramCount() : int
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, paramCount)
{
    ZEND_PARSE_PARAMETERS_NONE();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    UNSIGNED16 usCount = 0;
    UNSIGNED32 rc = AdsGetNumParams(obj->hStmt, &usCount);
    ADS_CHECK_RC(rc, "AdsGetNumParams");
    RETURN_LONG((zend_long)usCount);
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindString(string $name, string $value) : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindString)
{
    char   *name;   size_t name_len;
    char   *value;  size_t value_len;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name,  name_len)
        Z_PARAM_STRING(value, value_len)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetString(obj->hStmt,
                                    (UNSIGNED8 *)pname,
                                    (UNSIGNED8 *)value,
                                    (UNSIGNED32)value_len);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindString");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindInt(string $name, int $value) : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindInt)
{
    char      *name;  size_t name_len;
    zend_long  value;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, name_len)
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetLongLong(obj->hStmt,
                                      (UNSIGNED8 *)pname,
                                      (SIGNED64)value);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindInt");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindDouble(string $name, float $value) : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindDouble)
{
    char   *name;  size_t name_len;
    double  value;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, name_len)
        Z_PARAM_DOUBLE(value)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetDouble(obj->hStmt,
                                     (UNSIGNED8 *)pname,
                                     (DOUBLE)value);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindDouble");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindBool(string $name, bool $value) : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindBool)
{
    char *name;  size_t name_len;
    bool  value;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, name_len)
        Z_PARAM_BOOL(value)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetLogical(obj->hStmt,
                                      (UNSIGNED8 *)pname,
                                      (UNSIGNED16)(value ? 1 : 0));
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindBool");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindDate(string $name, string $value) : void
 * $value must be in CCYYMMDD format (e.g. "20260520").
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindDate)
{
    char *name;   size_t name_len;
    char *value;  size_t value_len;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name,  name_len)
        Z_PARAM_STRING(value, value_len)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetString(obj->hStmt,
                                    (UNSIGNED8 *)pname,
                                    (UNSIGNED8 *)value,
                                    (UNSIGNED32)value_len);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindDate");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindTimestamp(string $name, string $value) : void
 * $value format: "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS"
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindTimestamp)
{
    char *name;   size_t name_len;
    char *value;  size_t value_len;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name,  name_len)
        Z_PARAM_STRING(value, value_len)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    /* AdsSetString handles timestamp fields when passed a string value */
    UNSIGNED32 ulRet = AdsSetString(obj->hStmt,
                                     (UNSIGNED8 *)pname,
                                     (UNSIGNED8 *)value,
                                     (UNSIGNED32)value_len);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindTimestamp");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindMoney(string $name, int $value) : void
 *
 * ADS_MONEY stores values as a scaled SIGNED64 integer.
 * The scale is defined per-field in the table schema (typically 4 decimal
 * places, so $10.00 = 100000).  The caller is responsible for scaling.
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindMoney)
{
    char      *name;  size_t name_len;
    zend_long  value;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, name_len)
        Z_PARAM_LONG(value)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetMoney(obj->hStmt,
                                    (UNSIGNED8 *)pname,
                                    (SIGNED64)value);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindMoney");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindBinary(string $name, string $data,
 *                                  int $type = ADS_BINARY) : void
 *
 * Writes binary data into an ADS_BINARY (BLOB), ADS_IMAGE, or ADS_RAW
 * field parameter in a single call (ulOffset = 0, ulLen = total length).
 * $type defaults to ADS_BINARY (6); pass ADS_IMAGE (7) for image fields.
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindBinary)
{
    char      *name;  size_t name_len;
    char      *data;  size_t data_len;
    zend_long  type = ADS_BINARY;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_STRING(name, name_len)
        Z_PARAM_STRING(data, data_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(type)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetBinary(obj->hStmt,
                                     (UNSIGNED8 *)pname,
                                     (UNSIGNED16)type,
                                     (UNSIGNED32)data_len,
                                     0,
                                     (UNSIGNED8 *)data,
                                     (UNSIGNED32)data_len);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindBinary");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bindNull(string $name) : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bindNull)
{
    char *name;  size_t name_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AdsSetNull(obj->hStmt, (UNSIGNED8 *)pname);
    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bindNull");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::bind(string $name, mixed $value) : void
 *
 * Auto-detects PHP type:
 *   null   → AdsSetNull
 *   bool   → AdsSetLogical
 *   int    → AdsSetLong
 *   float  → AdsSetDouble
 *   string → AdsSetString
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, bind)
{
    char   *name;  size_t name_len;
    zval   *value;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, name_len)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    char nbuf[256];
    const char *pname = prep_param_name(name, nbuf, sizeof(nbuf));

    UNSIGNED32 ulRet = AE_SUCCESS;

    switch (Z_TYPE_P(value)) {
        case IS_NULL:
            ulRet = AdsSetNull(obj->hStmt, (UNSIGNED8 *)pname);
            break;

        case IS_TRUE:
            ulRet = AdsSetLogical(obj->hStmt, (UNSIGNED8 *)pname, 1);
            break;

        case IS_FALSE:
            ulRet = AdsSetLogical(obj->hStmt, (UNSIGNED8 *)pname, 0);
            break;

        case IS_LONG:
            ulRet = AdsSetLongLong(obj->hStmt, (UNSIGNED8 *)pname,
                                   (SIGNED64)Z_LVAL_P(value));
            break;

        case IS_DOUBLE:
            ulRet = AdsSetDouble(obj->hStmt, (UNSIGNED8 *)pname,
                                 (DOUBLE)Z_DVAL_P(value));
            break;

        case IS_STRING:
            ulRet = AdsSetString(obj->hStmt, (UNSIGNED8 *)pname,
                                 (UNSIGNED8 *)Z_STRVAL_P(value),
                                 (UNSIGNED32)Z_STRLEN_P(value));
            break;

        default:
            zend_throw_exception_ex(ads_exception_ce, 0,
                "AdsPreparedStatement::bind(): unsupported PHP type %d for parameter '%s'",
                (int)Z_TYPE_P(value), name);
            RETURN_THROWS();
    }

    ADS_CHECK_RC(ulRet, "AdsPreparedStatement::bind");
}

/* -----------------------------------------------------------------------
 * AdsPreparedStatement::execute() : AdsStatement|true
 *
 * Executes the prepared statement.
 * - SELECT queries  → returns an AdsStatement (cursor ready at first row)
 * - DML / DDL       → returns true
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsPreparedStatement, execute)
{
    ZEND_PARSE_PARAMETERS_NONE();

    ads_prepared_obj *obj = Z_ADS_PREP_P(ZEND_THIS);
    ADS_CHECK_PREP_CLOSED(obj);

    ADSHANDLE  hCursor = 0;
    UNSIGNED32 ulRet   = AdsExecuteSQL(obj->hStmt, &hCursor);

    if (ulRet != AE_SUCCESS) {
        ads_throw_ace_exception(ulRet, "AdsPreparedStatement::execute");
        RETURN_THROWS();
    }

    if (hCursor == 0) {
        /* DML / DDL — no result set */
        RETURN_TRUE;
    }

    /* SELECT — wrap cursor in AdsStatement */
    AdsGotoTop(hCursor);
    UNSIGNED16 numFields = 0;
    AdsGetNumFields(hCursor, &numFields);

    object_init_ex(return_value, ads_statement_ce);
    ads_statement_obj *stmt = Z_ADS_STMT_P(return_value);
    stmt->hStmt     = obj->hStmt;   /* statement handle shared */
    stmt->hCursor   = hCursor;
    stmt->numFields = numFields;
    stmt->executed  = 1;

    UNSIGNED16 pbEOF = 0;
    AdsAtEOF(hCursor, &pbEOF);
    stmt->eof = (zend_bool)pbEOF;

    /* Transfer ownership: prepared statement no longer owns the handle
     * while a result set is open.  Caller closes via AdsStatement::close(). */
    obj->hStmt  = 0;
    obj->closed = 1;
}

/* -----------------------------------------------------------------------
 * Method table
 * --------------------------------------------------------------------- */
static const zend_function_entry ads_prepared_methods[] = {
    PHP_ME(AdsPreparedStatement, bindString,    arginfo_ads_prepared_bind_string,    ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindInt,       arginfo_ads_prepared_bind_int,       ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindDouble,    arginfo_ads_prepared_bind_double,    ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindBool,      arginfo_ads_prepared_bind_bool,      ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindDate,      arginfo_ads_prepared_bind_date,      ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindTimestamp, arginfo_ads_prepared_bind_timestamp, ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindMoney,     arginfo_ads_prepared_bind_money,     ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindBinary,    arginfo_ads_prepared_bind_binary,    ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bindNull,      arginfo_ads_prepared_bind_null,      ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, bind,          arginfo_ads_prepared_bind,           ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, execute,       arginfo_ads_prepared_execute,        ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, paramCount,    arginfo_ads_prepared_param_count,    ZEND_ACC_PUBLIC)
    PHP_ME(AdsPreparedStatement, close,         arginfo_ads_prepared_close,          ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* -----------------------------------------------------------------------
 * Class registration (called from MINIT)
 * --------------------------------------------------------------------- */
void ads_prepared_register_class(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "AdsPreparedStatement", ads_prepared_methods);
    ads_prepared_ce = zend_register_internal_class(&ce);
    ads_prepared_ce->create_object = ads_prepared_create_object;

    memcpy(&ads_prepared_handlers,
           zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));
    ads_prepared_handlers.offset    = XtOffsetOf(ads_prepared_obj, std);
    ads_prepared_handlers.free_obj  = ads_prepared_free_object;
    ads_prepared_handlers.clone_obj = NULL;
}
