#include "php_ads.h"
#include "ads_arginfo.h"

/* =====================================================================
 * AdsTransaction
 * ===================================================================== */

zend_object_handlers ads_transaction_handlers;

static zend_object *ads_transaction_create_object(zend_class_entry *ce)
{
    ads_transaction_obj *obj = (ads_transaction_obj *)
        zend_object_alloc(sizeof(ads_transaction_obj), ce);
    obj->hConn  = 0;
    obj->active = 0;
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &ads_transaction_handlers;
    return &obj->std;
}

static void ads_transaction_free_object(zend_object *obj)
{
    ads_transaction_obj *intern = ads_trans_from_obj(obj);
    /* Auto-rollback if still active when GC'd */
    if (intern->active && intern->hConn != 0) {
        AdsRollbackTransaction(intern->hConn);
        intern->active = 0;
    }
    zend_object_std_dtor(obj);
}

/* -----------------------------------------------------------------------
 * AdsTransaction::commit() : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsTransaction, commit)
{
    ZEND_PARSE_PARAMETERS_NONE();

    ads_transaction_obj *trans = Z_ADS_TRANS_P(ZEND_THIS);
    if (!trans->active) {
        zend_throw_exception(ads_exception_ce,
            "AdsTransaction: no active transaction", 0);
        RETURN_THROWS();
    }

    UNSIGNED32 ulRet = AdsCommitTransaction(trans->hConn);
    trans->active = 0;

    if (ulRet != AE_SUCCESS) {
        ads_throw_ace_exception(ulRet, "AdsTransaction::commit");
        RETURN_THROWS();
    }
}

/* -----------------------------------------------------------------------
 * AdsTransaction::rollback() : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsTransaction, rollback)
{
    ZEND_PARSE_PARAMETERS_NONE();

    ads_transaction_obj *trans = Z_ADS_TRANS_P(ZEND_THIS);
    if (!trans->active) {
        zend_throw_exception(ads_exception_ce,
            "AdsTransaction: no active transaction", 0);
        RETURN_THROWS();
    }

    UNSIGNED32 ulRet = AdsRollbackTransaction(trans->hConn);
    trans->active = 0;

    if (ulRet != AE_SUCCESS) {
        ads_throw_ace_exception(ulRet, "AdsTransaction::rollback");
        RETURN_THROWS();
    }
}

/* -----------------------------------------------------------------------
 * AdsTransaction::isActive() : bool
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsTransaction, isActive)
{
    ZEND_PARSE_PARAMETERS_NONE();
    ads_transaction_obj *trans = Z_ADS_TRANS_P(ZEND_THIS);
    RETURN_BOOL(trans->active);
}

static const zend_function_entry ads_transaction_methods[] = {
    PHP_ME(AdsTransaction, commit,   arginfo_ads_transaction_commit,    ZEND_ACC_PUBLIC)
    PHP_ME(AdsTransaction, rollback, arginfo_ads_transaction_rollback,  ZEND_ACC_PUBLIC)
    PHP_ME(AdsTransaction, isActive, arginfo_ads_transaction_is_active, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* =====================================================================
 * AdsDictionary
 *
 * In ACE, when you connect to a .ADD file, the connection handle IS
 * the dictionary handle.  AdsDictionary wraps the connection handle
 * and exposes dictionary-level operations.
 * ===================================================================== */

zend_object_handlers ads_dictionary_handlers;

static zend_object *ads_dictionary_create_object(zend_class_entry *ce)
{
    ads_dictionary_obj *obj = (ads_dictionary_obj *)
        zend_object_alloc(sizeof(ads_dictionary_obj), ce);
    obj->hDict  = 0;
    obj->closed = 1;
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &ads_dictionary_handlers;
    return &obj->std;
}

static void ads_dictionary_free_object(zend_object *obj)
{
    ads_dictionary_obj *intern = ads_dict_from_obj(obj);
    if (!intern->closed && intern->hDict != 0) {
        AdsDisconnect(intern->hDict);
        intern->hDict  = 0;
        intern->closed = 1;
    }
    zend_object_std_dtor(obj);
}

/* -----------------------------------------------------------------------
 * AdsDictionary::open(string $dictPath, string $user="", string $pass="")
 *   : static
 *
 * Opens a .ADD dictionary file and returns an AdsDictionary object.
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsDictionary, open)
{
    char   *dictPath;
    size_t  dictPath_len;
    char   *user = "";
    size_t  user_len = 0;
    char   *pass = "";
    size_t  pass_len = 0;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STRING(dictPath, dictPath_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(user, user_len)
        Z_PARAM_STRING(pass, pass_len)
    ZEND_PARSE_PARAMETERS_END();

    ADSHANDLE  hConn  = 0;
    UNSIGNED32 ulRet  = AdsConnect60(
        (UNSIGNED8 *)dictPath,
        ADS_LOCAL_SERVER | ADS_REMOTE_SERVER,
        (UNSIGNED8 *)user,
        (UNSIGNED8 *)pass,
        0,
        &hConn
    );

    if (ulRet != AE_SUCCESS) {
        ads_throw_ace_exception(ulRet, "AdsDictionary::open");
        RETURN_THROWS();
    }

    object_init_ex(return_value, ads_dictionary_ce);
    ads_dictionary_obj *dict = Z_ADS_DICT_P(return_value);
    dict->hDict  = hConn;
    dict->closed = 0;
}

/* -----------------------------------------------------------------------
 * AdsDictionary::fromConnection(AdsConnection $conn) : static
 *
 * Returns an AdsDictionary view over an existing connection.
 * The dictionary does NOT own the handle — closing the dict does nothing.
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsDictionary, fromConnection)
{
    zval *zConn;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_OBJECT_OF_CLASS(zConn, ads_connection_ce)
    ZEND_PARSE_PARAMETERS_END();

    ads_connection_obj *conn = Z_ADS_CONN_P(zConn);
    ADS_CHECK_CONN_CLOSED(conn);

    object_init_ex(return_value, ads_dictionary_ce);
    ads_dictionary_obj *dict = Z_ADS_DICT_P(return_value);
    dict->hDict  = conn->hConn;
    dict->closed = 1;   /* 1 = borrowed: free_object won't disconnect */
}

/* -----------------------------------------------------------------------
 * AdsDictionary::close() : void
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsDictionary, close)
{
    ZEND_PARSE_PARAMETERS_NONE();

    ads_dictionary_obj *dict = Z_ADS_DICT_P(ZEND_THIS);
    if (!dict->closed && dict->hDict != 0) {
        AdsDisconnect(dict->hDict);
        dict->hDict  = 0;
        dict->closed = 1;
    }
}

/* -----------------------------------------------------------------------
 * AdsDictionary::getTableProperty(string $tableName, int $property) : string
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsDictionary, getTableProperty)
{
    char      *tableName;
    size_t     tableName_len;
    zend_long  property;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(tableName, tableName_len)
        Z_PARAM_LONG(property)
    ZEND_PARSE_PARAMETERS_END();

    ads_dictionary_obj *dict = Z_ADS_DICT_P(ZEND_THIS);
    if (dict->hDict == 0) {
        zend_throw_exception(ads_exception_ce, "AdsDictionary is closed", 0);
        RETURN_THROWS();
    }

    char       buf[1024];
    UNSIGNED16 usLen = (UNSIGNED16)(sizeof(buf) - 1);
    UNSIGNED32 ulRet = AdsDDGetTableProperty(
        dict->hDict,
        (UNSIGNED8 *)tableName,
        (UNSIGNED16)property,
        (UNSIGNED8 *)buf,
        &usLen
    );

    if (ulRet != AE_SUCCESS) {
        ads_throw_ace_exception(ulRet, "AdsDictionary::getTableProperty");
        RETURN_THROWS();
    }
    buf[usLen] = '\0';
    RETURN_STRINGL(buf, (size_t)usLen);
}

/* -----------------------------------------------------------------------
 * AdsDictionary::getDatabaseProperty(int $property) : string
 * --------------------------------------------------------------------- */
PHP_METHOD(AdsDictionary, getDatabaseProperty)
{
    zend_long property;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(property)
    ZEND_PARSE_PARAMETERS_END();

    ads_dictionary_obj *dict = Z_ADS_DICT_P(ZEND_THIS);
    if (dict->hDict == 0) {
        zend_throw_exception(ads_exception_ce, "AdsDictionary is closed", 0);
        RETURN_THROWS();
    }

    char       buf[1024];
    UNSIGNED16 usLen = (UNSIGNED16)(sizeof(buf) - 1);
    UNSIGNED32 ulRet = AdsDDGetDatabaseProperty(
        dict->hDict,
        (UNSIGNED16)property,
        (UNSIGNED8 *)buf,
        &usLen
    );

    if (ulRet != AE_SUCCESS) {
        ads_throw_ace_exception(ulRet, "AdsDictionary::getDatabaseProperty");
        RETURN_THROWS();
    }
    buf[usLen] = '\0';
    RETURN_STRINGL(buf, (size_t)usLen);
}

static const zend_function_entry ads_dictionary_methods[] = {
    PHP_ME(AdsDictionary, open,                arginfo_ads_dictionary_open,                 ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(AdsDictionary, fromConnection,      arginfo_ads_dictionary_from_connection,      ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(AdsDictionary, close,               arginfo_ads_dictionary_close,                ZEND_ACC_PUBLIC)
    PHP_ME(AdsDictionary, getTableProperty,    arginfo_ads_dictionary_get_table_property,   ZEND_ACC_PUBLIC)
    PHP_ME(AdsDictionary, getDatabaseProperty, arginfo_ads_dictionary_get_database_property, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* =====================================================================
 * Combined registration
 * ===================================================================== */
void ads_misc_register_classes(void)
{
    zend_class_entry ce;

    /* AdsTransaction */
    INIT_CLASS_ENTRY(ce, "AdsTransaction", ads_transaction_methods);
    ads_transaction_ce = zend_register_internal_class(&ce);
    ads_transaction_ce->create_object = ads_transaction_create_object;

    memcpy(&ads_transaction_handlers,
           zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));
    ads_transaction_handlers.offset    = XtOffsetOf(ads_transaction_obj, std);
    ads_transaction_handlers.free_obj  = ads_transaction_free_object;
    ads_transaction_handlers.clone_obj = NULL;

    /* AdsDictionary */
    INIT_CLASS_ENTRY(ce, "AdsDictionary", ads_dictionary_methods);
    ads_dictionary_ce = zend_register_internal_class(&ce);
    ads_dictionary_ce->create_object = ads_dictionary_create_object;

    memcpy(&ads_dictionary_handlers,
           zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));
    ads_dictionary_handlers.offset    = XtOffsetOf(ads_dictionary_obj, std);
    ads_dictionary_handlers.free_obj  = ads_dictionary_free_object;
    ads_dictionary_handlers.clone_obj = NULL;
}
