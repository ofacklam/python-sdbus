// SPDX-License-Identifier: LGPL-2.1-or-later
/*
    Copyright (C) 2020  igo95862

    This file is part of py_sd_bus

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <systemd/sd-bus.h>
#include <structmember.h>

#define SD_BUS_PY_CHECK_RETURN_VALUE(_exception_to_raise)                 \
    if (return_value < 0)                                                 \
    {                                                                     \
        PyErr_Format(_exception_to_raise, "sd-bus returned error %i: %s", \
                     -return_value, strerror(-return_value));             \
        return NULL;                                                      \
    }

#define PY_SD_BUS_CHECK_ARGS_NUMBER(number_args)                                             \
    if (nargs != number_args)                                                                \
    {                                                                                        \
        PyErr_Format(PyExc_TypeError, "Expected " #number_args " arguments, got %i", nargs); \
        return NULL;                                                                         \
    }

#define PY_SD_BUS_CHECK_ARG_TYPE(arg_num, arg_expected_type)                           \
    if (Py_TYPE(args[arg_num]) != &arg_expected_type)                                  \
    {                                                                                  \
        PyErr_SetString(PyExc_TypeError, "Argument is not an " #arg_expected_type ""); \
        return NULL;                                                                   \
    }

static PyObject *exception_dict = NULL;
static PyObject *exception_default = NULL;
static PyObject *exception_generic = NULL;
static PyTypeObject *async_future_type = NULL;

typedef struct
{
    PyObject_HEAD;
    sd_bus_message *message_ref;
} SdBusMessageObject;

static int
SdBusMessage_init(SdBusMessageObject *self, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    self->message_ref = NULL;
    return 0;
}

static void
SdBusMessage_free(SdBusMessageObject *self)
{
    sd_bus_message_unref(self->message_ref);
}

static PyObject *
SdBusMessage_dump(SdBusMessageObject *self, PyObject *Py_UNUSED(args))
{
    int return_value = sd_bus_message_dump(self->message_ref, 0, SD_BUS_MESSAGE_DUMP_WITH_HEADER);
    SD_BUS_PY_CHECK_RETURN_VALUE(PyExc_RuntimeError);
    return_value = sd_bus_message_rewind(self->message_ref, 1);
    SD_BUS_PY_CHECK_RETURN_VALUE(PyExc_RuntimeError);
    return Py_None;
}

static PyObject *
SdBusMessage_add_str(SdBusMessageObject *self,
                     PyObject *const *args,
                     Py_ssize_t nargs)
{
    PY_SD_BUS_CHECK_ARGS_NUMBER(1);
    PY_SD_BUS_CHECK_ARG_TYPE(0, PyUnicode_Type);

    sd_bus_message_append_basic(self->message_ref, 's', PyUnicode_AsUTF8(args[0]));
    Py_RETURN_NONE;
}

static PyMethodDef SdBusMessage_methods[] = {
    {"add_str", (void *)SdBusMessage_add_str, METH_FASTCALL, "Add str to message"},
    {"dump", (PyCFunction)SdBusMessage_dump, METH_NOARGS, "Dump message to stdout"},
    {NULL, NULL, 0, NULL},
};

static PyTypeObject SdBusMessageType = {
    PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "sd_bus_internals.SdBusMessage",
    .tp_basicsize = sizeof(SdBusMessageObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)SdBusMessage_init,
    .tp_free = (freefunc)SdBusMessage_free,
    .tp_methods = SdBusMessage_methods,
};

typedef struct
{
    PyObject_HEAD;
    sd_bus *sd_bus_ref;
} SdBusObject;

static void
SdBus_free(SdBusObject *self)
{
    sd_bus_unref(self->sd_bus_ref);
}

static int
SdBus_init(SdBusObject *self, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    self->sd_bus_ref = NULL;
    return 0;
}

static SdBusMessageObject *
SdBus_new_method_call_message(SdBusObject *self,
                              PyObject *const *args,
                              Py_ssize_t nargs)
{
    PY_SD_BUS_CHECK_ARGS_NUMBER(4);
    PY_SD_BUS_CHECK_ARG_TYPE(0, PyUnicode_Type);
    PY_SD_BUS_CHECK_ARG_TYPE(1, PyUnicode_Type);
    PY_SD_BUS_CHECK_ARG_TYPE(2, PyUnicode_Type);
    PY_SD_BUS_CHECK_ARG_TYPE(3, PyUnicode_Type);

    const char *destination_bus_name = PyUnicode_AsUTF8(args[0]);
    const char *object_path = PyUnicode_AsUTF8(args[1]);
    const char *interface_name = PyUnicode_AsUTF8(args[2]);
    const char *member_name = PyUnicode_AsUTF8(args[3]);

    SdBusMessageObject *new_message_object = PyObject_NEW(SdBusMessageObject, &SdBusMessageType);
    SdBusMessageType.tp_init((PyObject *)new_message_object, NULL, NULL);
    int return_value = sd_bus_message_new_method_call(
        self->sd_bus_ref,
        &new_message_object->message_ref,
        destination_bus_name,
        object_path,
        interface_name,
        member_name);
    SD_BUS_PY_CHECK_RETURN_VALUE(PyExc_RuntimeError); // TODO: decrease reference
    return new_message_object;
}

static SdBusMessageObject *
SdBus_call(SdBusObject *self,
           PyObject *const *args,
           Py_ssize_t nargs)
{
    PY_SD_BUS_CHECK_ARGS_NUMBER(1);
    PY_SD_BUS_CHECK_ARG_TYPE(0, SdBusMessageType);

    SdBusMessageObject *call_message = (SdBusMessageObject *)args[0];

    SdBusMessageObject *reply_message_object = PyObject_NEW(SdBusMessageObject, &SdBusMessageType);
    SdBusMessageType.tp_init((PyObject *)reply_message_object, NULL, NULL);

    sd_bus_error error __attribute__((cleanup(sd_bus_error_free))) = SD_BUS_ERROR_NULL;

    int return_value = sd_bus_call(
        self->sd_bus_ref,
        call_message->message_ref,
        (uint64_t)0,
        &error,
        &reply_message_object->message_ref);

    if (error.name != NULL)
    {
        PyObject *exception_to_raise = PyDict_GetItemString(exception_dict, error.name);
        if (exception_to_raise == NULL)
        {
            exception_to_raise = exception_generic;
        }
        PyErr_SetObject(exception_to_raise, Py_BuildValue("(ss)", error.name, error.message));
        return NULL;
    }
    else
    {
        SD_BUS_PY_CHECK_RETURN_VALUE(PyExc_RuntimeError);
    }

    return reply_message_object;
}

int PySbBus_async_callback(sd_bus_message *m,
                           void *userdata, // Should be the asyncio.Future
                           sd_bus_error *Py_UNUSED(ret_error))
{

    if (!sd_bus_message_is_method_error(m, NULL))
    {
        // Not Error, set Future result to new message object
        SdBusMessageObject *reply_message_object = PyObject_NEW(SdBusMessageObject, &SdBusMessageType);
        SdBusMessageType.tp_init((PyObject *)reply_message_object, NULL, NULL);
        PyObject *return_object = PyObject_CallMethod(userdata, "set_result", "O", reply_message_object);
        if (return_object == NULL)
        {
            return -1;
        }
    }
    else
    {
        // An Error, set exception
        const sd_bus_error *callback_error = sd_bus_message_get_error(m);
        PyObject *exception_to_raise = PyDict_GetItemString(exception_dict, callback_error->name);
        if (exception_to_raise == NULL)
        {
            exception_to_raise = exception_generic;
        }
        PyErr_SetObject(exception_to_raise, Py_BuildValue("(ss)", callback_error->name, callback_error->message));
        PyObject *return_object = PyObject_CallMethod(userdata, "set_exception", "O", exception_to_raise);
        if (return_object == NULL)
        {
            return -1;
        }
    }
    return 0;
}

PyObject *create_future()
{
    PyObject *dummy_tuple = PyTuple_New(0);
    PyObject *dummy_dict = PyDict_New();
    PyObject *reply_future_object = PyObject_Call((PyObject *)async_future_type, dummy_tuple, dummy_dict);
    Py_XDECREF(dummy_tuple);
    Py_XDECREF(dummy_dict);
    return reply_future_object;
}

static PyObject *
SdBus_call_async(SdBusObject *self,
                 PyObject *const *args,
                 Py_ssize_t nargs)
{
    PY_SD_BUS_CHECK_ARGS_NUMBER(1);
    PY_SD_BUS_CHECK_ARG_TYPE(0, SdBusMessageType);

    SdBusMessageObject *call_message = (SdBusMessageObject *)args[0];

    PyObject *reply_future_object = create_future();
    if (reply_future_object == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create future");
    }
    sd_bus_error error __attribute__((cleanup(sd_bus_error_free))) = SD_BUS_ERROR_NULL;

    int return_value = sd_bus_call_async(
        self->sd_bus_ref,
        NULL, // TODO: make cancelable
        call_message->message_ref,
        PySbBus_async_callback,
        reply_future_object,
        (uint64_t)0);

    SD_BUS_PY_CHECK_RETURN_VALUE(PyExc_RuntimeError);

    return reply_future_object;
}

static PyMethodDef SdBus_methods[] = {
    {"call", (void *)SdBus_call, METH_FASTCALL, "Send message and get reply"},
    {"call_async", (void *)SdBus_call_async, METH_FASTCALL, "Async send message, returns awaitable future"},
    {"new_method_call_message", (void *)SdBus_new_method_call_message, METH_FASTCALL, NULL},
    {NULL, NULL, 0, NULL},
};

static PyTypeObject SdBusType = {
    PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "sd_bus_internals.SdBus",
    .tp_basicsize = sizeof(SdBusObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)SdBus_init,
    .tp_free = (freefunc)SdBus_free,
    .tp_methods = SdBus_methods,
};

static SdBusObject *
get_default_sd_bus(PyObject *Py_UNUSED(self),
                   PyObject *Py_UNUSED(ignored))
{
    SdBusObject *new_sd_bus = PyObject_New(SdBusObject, &SdBusType);
    SdBusType.tp_init((PyObject *)new_sd_bus, NULL, NULL);
    int return_value = sd_bus_default(&(new_sd_bus->sd_bus_ref));
    SD_BUS_PY_CHECK_RETURN_VALUE(PyExc_RuntimeError);
    return new_sd_bus;
}

static PyMethodDef SdBusPyInternal_methods[] = {
    {"get_default_sdbus", (PyCFunction)get_default_sd_bus, METH_NOARGS, "Get default sd_bus."},
    {NULL, NULL, 0, NULL},
};

static PyModuleDef sd_bus_internals_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "sd_bus_internals",
    .m_doc = "Sd bus internals module.",
    .m_methods = SdBusPyInternal_methods,
    .m_size = -1,
};

PyMODINIT_FUNC
PyInit_sd_bus_internals(void)
{
    PyObject *m;
    if (PyType_Ready(&SdBusType) < 0)
    {
        return NULL;
    }
    if (PyType_Ready(&SdBusMessageType) < 0)
    {
        return NULL;
    }

    m = PyModule_Create(&sd_bus_internals_module);
    if (m == NULL)
    {
        return NULL;
    }

    Py_INCREF(&SdBusType);
    if (PyModule_AddObject(m, "SdBus", (PyObject *)&SdBusType) < 0)
    {
        Py_DECREF(&SdBusType);
        Py_DECREF(m);
        return NULL;
    }
    if (PyModule_AddObject(m, "SdBusMessage", (PyObject *)&SdBusMessageType) < 0)
    {
        Py_DECREF(&SdBusMessageType);
        Py_DECREF(m);
        return NULL;
    }

    // Exception map
    exception_dict = PyDict_New();
    if (exception_dict == NULL)
    {
        Py_DECREF(m);
        return NULL;
    }
    if (PyModule_AddObject(m, "_ExceptionsMap", exception_dict) < 0)
    {
        Py_XDECREF(exception_dict);
        Py_DECREF(m);
        return NULL;
    }

    // TODO: check if PyErr_NewException can return NULL
    PyObject *new_base_exception = PyErr_NewException("sd_bus_internals.DbusBaseError", NULL, NULL);
    if (PyModule_AddObject(m, "DbusBaseError", new_base_exception) < 0)
    {
        Py_XDECREF(new_base_exception);
        Py_DECREF(m);
        return NULL;
    }
    exception_default = new_base_exception;

    PyObject *new_exception = NULL;
#define PY_SD_BUS_ADD_EXCEPTION(exception_name, dbus_string)                                              \
    new_exception = PyErr_NewException("sd_bus_internals." #exception_name "", new_base_exception, NULL); \
    PyDict_SetItemString(exception_dict, dbus_string, new_exception);                                     \
    if (PyModule_AddObject(m, #exception_name, new_exception) < 0)                                        \
    {                                                                                                     \
        Py_XDECREF(new_exception);                                                                        \
        Py_DECREF(m);                                                                                     \
        return NULL;                                                                                      \
    }

    PY_SD_BUS_ADD_EXCEPTION(DbusFailedError, SD_BUS_ERROR_FAILED);
    PY_SD_BUS_ADD_EXCEPTION(DbusNoMemoryError, SD_BUS_ERROR_NO_MEMORY);
    PY_SD_BUS_ADD_EXCEPTION(DbusServiceUnknownError, SD_BUS_ERROR_SERVICE_UNKNOWN);
    PY_SD_BUS_ADD_EXCEPTION(DbusNameHasNoOwnerError, SD_BUS_ERROR_NAME_HAS_NO_OWNER);
    PY_SD_BUS_ADD_EXCEPTION(DbusNoReplyError, SD_BUS_ERROR_NO_REPLY);
    PY_SD_BUS_ADD_EXCEPTION(DbusIOError, SD_BUS_ERROR_IO_ERROR);
    PY_SD_BUS_ADD_EXCEPTION(DbusBadAddressError, SD_BUS_ERROR_BAD_ADDRESS);
    PY_SD_BUS_ADD_EXCEPTION(DbusNotSupportedError, SD_BUS_ERROR_NOT_SUPPORTED);
    PY_SD_BUS_ADD_EXCEPTION(DbusLimitsExceededError, SD_BUS_ERROR_LIMITS_EXCEEDED);
    PY_SD_BUS_ADD_EXCEPTION(DbusAccessDeniedError, SD_BUS_ERROR_ACCESS_DENIED);
    PY_SD_BUS_ADD_EXCEPTION(DbusAuthFailedError, SD_BUS_ERROR_AUTH_FAILED);
    PY_SD_BUS_ADD_EXCEPTION(DbusNoServerError, SD_BUS_ERROR_NO_SERVER);
    PY_SD_BUS_ADD_EXCEPTION(DbusTimeoutError, SD_BUS_ERROR_TIMEOUT);
    PY_SD_BUS_ADD_EXCEPTION(DbusNoNetworkError, SD_BUS_ERROR_NO_NETWORK);
    PY_SD_BUS_ADD_EXCEPTION(DbusAddressInUseError, SD_BUS_ERROR_ADDRESS_IN_USE);
    PY_SD_BUS_ADD_EXCEPTION(DbusDisconnectedError, SD_BUS_ERROR_DISCONNECTED);
    PY_SD_BUS_ADD_EXCEPTION(DbusInvalidArgsError, SD_BUS_ERROR_INVALID_ARGS);
    PY_SD_BUS_ADD_EXCEPTION(DbusFileExistsError, SD_BUS_ERROR_FILE_EXISTS);
    PY_SD_BUS_ADD_EXCEPTION(DbusUnknownMethodError, SD_BUS_ERROR_UNKNOWN_METHOD);
    PY_SD_BUS_ADD_EXCEPTION(DbusUnknownObjectError, SD_BUS_ERROR_UNKNOWN_OBJECT);
    PY_SD_BUS_ADD_EXCEPTION(DbusUnknownInterfaceError, SD_BUS_ERROR_UNKNOWN_INTERFACE);
    PY_SD_BUS_ADD_EXCEPTION(DbusUnknownPropertyError, SD_BUS_ERROR_UNKNOWN_PROPERTY);
    PY_SD_BUS_ADD_EXCEPTION(DbusPropertyReadOnlyError, SD_BUS_ERROR_PROPERTY_READ_ONLY);
    PY_SD_BUS_ADD_EXCEPTION(DbusUnixProcessIdUnknownError, SD_BUS_ERROR_UNIX_PROCESS_ID_UNKNOWN);
    PY_SD_BUS_ADD_EXCEPTION(DbusInvalidSignatureError, SD_BUS_ERROR_INVALID_SIGNATURE);
    PY_SD_BUS_ADD_EXCEPTION(DbusInconsistentMessageError, SD_BUS_ERROR_INCONSISTENT_MESSAGE);
    PY_SD_BUS_ADD_EXCEPTION(DbusMatchRuleNotFound, SD_BUS_ERROR_MATCH_RULE_NOT_FOUND);
    PY_SD_BUS_ADD_EXCEPTION(DbusMatchRuleInvalidError, SD_BUS_ERROR_MATCH_RULE_INVALID);
    PY_SD_BUS_ADD_EXCEPTION(DbusInteractiveAuthorizationRequiredError, SD_BUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED);

    exception_generic = PyErr_NewException("sd_bus_internals.DbusGenericError", new_base_exception, NULL);
    if (PyModule_AddObject(m, "DbusGenericError", exception_generic) < 0)
    {
        Py_XDECREF(exception_generic);
        Py_DECREF(m);
        return NULL;
    }

    PyObject *asyncio_module = PyImport_ImportModule("asyncio");
    async_future_type = (PyTypeObject *)PyObject_GetAttrString(asyncio_module, "Future");
    if (async_future_type == NULL)
    {
        Py_DECREF(m);
        return NULL;
    }
    if (!PyType_Check(async_future_type))
    {
        Py_DECREF(m);
        return NULL;
    }
    if (PyType_Ready(async_future_type) < 0)
    {
        return NULL;
    }

    return m;
}