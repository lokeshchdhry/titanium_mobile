/**
 * Appcelerator Titanium Mobile
 * Copyright (c) 2009-2016 by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */
#include <jni.h>
#include <stdio.h>
#include <v8.h>

#include "AndroidUtil.h"
#include "EventEmitter.h"
#include "JNIUtil.h"
#include "TypeConverter.h"
#include "Proxy.h"
#include "ProxyFactory.h"
#include "V8Runtime.h"
#include "V8Util.h"

#include "org_appcelerator_kroll_runtime_v8_V8Object.h"

#define TAG "V8Object"

using namespace titanium;
using namespace v8;

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_org_appcelerator_kroll_runtime_v8_V8Object_nativeInitObject
	(JNIEnv *env, jclass clazz, jclass proxyClass, jobject proxyObject)
{
	HandleScope scope(V8Runtime::v8_isolate);
	JNIScope jniScope(env);

	ProxyFactory::createV8Proxy(V8Runtime::v8_isolate, proxyClass, proxyObject);
}

JNIEXPORT void JNICALL
Java_org_appcelerator_kroll_runtime_v8_V8Object_nativeSetProperty
	(JNIEnv *env, jobject object, jlong ptr, jstring name, jobject value)
{
	HandleScope scope(V8Runtime::v8_isolate);
	titanium::JNIScope jniScope(env);

	Local<Object> jsObject;
	if (ptr != 0) {
		titanium::Proxy* proxy = (titanium::Proxy*) ptr;
		jsObject = proxy->handle(V8Runtime::v8_isolate);
	} else {
		jsObject = TypeConverter::javaObjectToJsValue(V8Runtime::v8_isolate, env, object).As<Object>();
	}

	Local<Object> properties = jsObject->Get(titanium::Proxy::propertiesSymbol.Get(V8Runtime::v8_isolate)).As<Object>();
	Local<Value> jsName = TypeConverter::javaStringToJsString(V8Runtime::v8_isolate, env, name);
	Local<Value> jsValue = TypeConverter::javaObjectToJsValue(V8Runtime::v8_isolate, env, value);

	jsObject->SetAccessor(jsName->ToString(V8Runtime::v8_isolate), titanium::Proxy::getProperty, titanium::Proxy::onPropertyChanged);
	properties->Set(jsName, jsValue);
}


JNIEXPORT jboolean JNICALL
Java_org_appcelerator_kroll_runtime_v8_V8Object_nativeFireEvent
	(JNIEnv *env, jobject jEmitter, jlong ptr, jobject jsource, jlong sourcePtr, jstring event, jobject data, jboolean bubble, jboolean reportSuccess, jint code, jstring errorMessage)
{
	HandleScope scope(V8Runtime::v8_isolate);
	JNIScope jniScope(env);

	Local<Value> jsEvent = TypeConverter::javaStringToJsString(V8Runtime::v8_isolate, env, event);

#ifdef TI_DEBUG
	titanium::Utf8Value eventName(jsEvent);
	LOGV(TAG, "firing event \"%s\"", *eventName);
#endif

	Local<Object> emitter;
	if (ptr != 0) {
		titanium::Proxy* proxy = (titanium::Proxy*) ptr;
		emitter = proxy->handle(V8Runtime::v8_isolate);
	} else {
		emitter = TypeConverter::javaObjectToJsValue(V8Runtime::v8_isolate, env, jEmitter).As<Object>();
	}

	Local<Value> fireEventValue = emitter->Get(EventEmitter::emitSymbol.Get(V8Runtime::v8_isolate));
	if (!fireEventValue->IsFunction()) {
		return JNI_FALSE;
	}

	Local<Object> source;
	if ((jsource == NULL) || (jsource == jEmitter)) {
		source = emitter;
	} else if (sourcePtr != 0) {
		titanium::Proxy* proxy = (titanium::Proxy*) sourcePtr;
		source = proxy->handle(V8Runtime::v8_isolate);
	} else {
		source = TypeConverter::javaObjectToJsValue(V8Runtime::v8_isolate, env, jsource).As<Object>();
	}

	Local<Function> fireEvent = fireEventValue.As<Function>();

	Local<Object> jsData = TypeConverter::javaHashMapToJsValue(V8Runtime::v8_isolate, env, data);

	jsData->Set(NEW_SYMBOL(V8Runtime::v8_isolate, "bubbles"), TypeConverter::javaBooleanToJsBoolean(V8Runtime::v8_isolate, bubble));

	jsData->Set(NEW_SYMBOL(V8Runtime::v8_isolate, "source"), source);

	if (reportSuccess || code != 0) {
		jsData->Set(NEW_SYMBOL(V8Runtime::v8_isolate, "success"), TypeConverter::javaBooleanToJsBoolean(V8Runtime::v8_isolate, code == 0));
		jsData->Set(NEW_SYMBOL(V8Runtime::v8_isolate, "code"), TypeConverter::javaIntToJsNumber(V8Runtime::v8_isolate, code));
	}

	if (errorMessage != NULL) {
		jsData->Set(NEW_SYMBOL(V8Runtime::v8_isolate, "error"), TypeConverter::javaStringToJsString(V8Runtime::v8_isolate, env, errorMessage));
	}

	TryCatch tryCatch(V8Runtime::v8_isolate);
	Local<Value> args[] = { jsEvent, jsData };
	MaybeLocal<Value> result = fireEvent->Call(V8Runtime::v8_isolate->GetCurrentContext(), emitter, 2, args);

	if (tryCatch.HasCaught()) {
		V8Util::openJSErrorDialog(V8Runtime::v8_isolate, tryCatch);
		V8Util::reportException(V8Runtime::v8_isolate, tryCatch);
	} else if (result.IsEmpty()) {
		return JNI_FALSE;
	} else if (result.ToLocalChecked()->IsTrue()) {
		return JNI_TRUE;
	}
	return JNI_FALSE;
}

JNIEXPORT jobject JNICALL
Java_org_appcelerator_kroll_runtime_v8_V8Object_nativeCallProperty
	(JNIEnv* env, jclass clazz, jlong ptr, jstring propertyName, jobjectArray args)
{
	HandleScope scope(V8Runtime::v8_isolate);
	JNIScope jniScope(env);

	Local<Value> jsPropertyName = TypeConverter::javaStringToJsString(V8Runtime::v8_isolate, env, propertyName);

	titanium::Proxy* proxy = (titanium::Proxy*) ptr;
	Local<Object> object = proxy->handle(V8Runtime::v8_isolate);
	Local<Value> property = object->Get(jsPropertyName);
	if (!property->IsFunction()) {
		return JNIUtil::undefinedObject;
	}

	int argc = 0;
	Local<Value>* argv = NULL;
	if (args) {
		argv = TypeConverter::javaObjectArrayToJsArguments(V8Runtime::v8_isolate, args, &argc);
	}

	TryCatch tryCatch(V8Runtime::v8_isolate);
	Local<Function> function = property.As<Function>();
	MaybeLocal<Value> returnValue = function->Call(V8Runtime::v8_isolate->GetCurrentContext(), object, argc, argv);

	if (argv) {
		delete[] argv;
	}

	if (tryCatch.HasCaught()) {
		V8Util::openJSErrorDialog(V8Runtime::v8_isolate, tryCatch);
		V8Util::reportException(V8Runtime::v8_isolate, tryCatch);
		return JNIUtil::undefinedObject;
	} else if (returnValue.IsEmpty()) {
		return JNIUtil::undefinedObject;
	}

	bool isNew;
	return TypeConverter::jsValueToJavaObject(V8Runtime::v8_isolate, env, returnValue.ToLocalChecked(), &isNew);
}

JNIEXPORT jboolean JNICALL
Java_org_appcelerator_kroll_runtime_v8_V8Object_nativeRelease
	(JNIEnv *env, jclass clazz, jlong refPointer)
{
	HandleScope scope(V8Runtime::v8_isolate);
	JNIScope jniScope(env);

	if (refPointer) {
		titanium::Proxy* proxy = (titanium::Proxy*) refPointer;
		if (proxy && proxy->isDetached()) {
			delete proxy;
			return true;
		}
	}

	return false;
}

JNIEXPORT void JNICALL
Java_org_appcelerator_kroll_runtime_v8_V8Object_nativeSetWindow
	(JNIEnv *env, jobject javaKrollWindow, jlong ptr, jobject javaWindow)
{
	HandleScope scope(V8Runtime::v8_isolate);
	titanium::JNIScope jniScope(env);

	Local<Object> jsKrollWindow;
	if (ptr != 0) {
		titanium::Proxy* proxy = (titanium::Proxy*) ptr;
		jsKrollWindow = proxy->handle(V8Runtime::v8_isolate);
	} else {
		jsKrollWindow = TypeConverter::javaObjectToJsValue(V8Runtime::v8_isolate, env, javaKrollWindow).As<Object>();
	}

	Local<Value> setWindowValue = jsKrollWindow->Get(STRING_NEW(V8Runtime::v8_isolate, "setWindow"));
	if (!setWindowValue->IsFunction()) {
		return;
	}

	Local<Function> setWindow = setWindowValue.As<Function>();

	Local<Value> jsWindow = TypeConverter::javaObjectToJsValue(V8Runtime::v8_isolate, env, javaWindow);

	TryCatch tryCatch(V8Runtime::v8_isolate);
	if (!jsWindow->IsNull()) {
		Local<Value> args[] = { jsWindow };
		setWindow->Call(V8Runtime::v8_isolate->GetCurrentContext(), jsKrollWindow, 1, args);
	}

	if (tryCatch.HasCaught()) {
		V8Util::openJSErrorDialog(V8Runtime::v8_isolate, tryCatch);
		V8Util::reportException(V8Runtime::v8_isolate, tryCatch);
	}
}

#ifdef __cplusplus
}
#endif
