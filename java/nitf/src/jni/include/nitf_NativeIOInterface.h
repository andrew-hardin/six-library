/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class nitf_NativeIOInterface */

#ifndef _Included_nitf_NativeIOInterface
#define _Included_nitf_NativeIOInterface
#ifdef __cplusplus
extern "C" {
#endif
#undef nitf_NativeIOInterface_INVALID_ADDRESS
#define nitf_NativeIOInterface_INVALID_ADDRESS 0L
#undef nitf_NativeIOInterface_SEEK_CUR
#define nitf_NativeIOInterface_SEEK_CUR 10L
#undef nitf_NativeIOInterface_SEEK_SET
#define nitf_NativeIOInterface_SEEK_SET 20L
#undef nitf_NativeIOInterface_SEEK_END
#define nitf_NativeIOInterface_SEEK_END 30L
/*
 * Class:     nitf_NativeIOInterface
 * Method:    read
 * Signature: ([BI)V
 */
JNIEXPORT void JNICALL Java_nitf_NativeIOInterface_read
  (JNIEnv *, jobject, jbyteArray, jint);

/*
 * Class:     nitf_NativeIOInterface
 * Method:    write
 * Signature: ([BI)V
 */
JNIEXPORT void JNICALL Java_nitf_NativeIOInterface_write
  (JNIEnv *, jobject, jbyteArray, jint);

/*
 * Class:     nitf_NativeIOInterface
 * Method:    seek
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_nitf_NativeIOInterface_seek
  (JNIEnv *, jobject, jlong, jint);

/*
 * Class:     nitf_NativeIOInterface
 * Method:    tell
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_nitf_NativeIOInterface_tell
  (JNIEnv *, jobject);

/*
 * Class:     nitf_NativeIOInterface
 * Method:    getSize
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_nitf_NativeIOInterface_getSize
  (JNIEnv *, jobject);

/*
 * Class:     nitf_NativeIOInterface
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_nitf_NativeIOInterface_close
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif