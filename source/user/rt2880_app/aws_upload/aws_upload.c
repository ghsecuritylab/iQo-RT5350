/*
 * Qodome iQo push data into cloud
 */

#include "WWWLib.h"
#include "WWWInit.h"
#include "jansson.h"

json_t *json = NULL;
json_t *json_fn = NULL;

PRIVATE int printer (const char * fmt, va_list pArgs)
{
    return (vfprintf(stdout, fmt, pArgs));
}

PRIVATE int tracer (const char * fmt, va_list pArgs)
{
    return (vfprintf(stderr, fmt, pArgs));
}

PRIVATE int step1_terminate_handler (HTRequest * request, HTResponse * response,
			       void * param, int status) 
{
    json_error_t error;
    HTChunk * chunk = (HTChunk *) HTRequest_context(request);

    /* Check for status */
    if (status != HT_LOADED) {
        HTPrint("Error: step1 returned status %d\n", status);
        goto err;
    }

    json = json_loads(HTChunk_data(chunk), 0, &error);
    if (json == NULL) {
        HTPrint("Error: json failed to decode step1 results\n");
    } else {
        if (json_is_object(json)) {
            const char *key;
            json_t *value;

            json_object_foreach(json, key, value) {
                if (key == NULL || json_string_value(value) == NULL) {
                    HTPrint("Step1 parameter not valid!\n");
                    goto err;
                }
                HTPrint("%s: %s\n", key, json_string_value(value));
                if (strcmp(key, "key") == 0) {
                    json_fn = value;
                }
            }
        } else {
            HTPrint("Parameter returned from step1 not in JSON obj format\n");
            goto err;
        }
    }

    if (chunk) {
        HTChunk_delete(chunk);
    }

    HTEventList_stopLoop();
    return HT_OK;
err:
    if (chunk) {
        HTChunk_delete(chunk);
    }
    if (json != NULL) {
        json_decref(json);
    }
    HTRequest_delete(request);
	HTProfile_delete();
    exit(-1);
}

PRIVATE int step2_terminate_handler (HTRequest * request, HTResponse * response,
			       void * param, int status) 
{
    if (status != 201) {
        HTPrint("Error: step2 returned status %d\n", status);
        goto err;
    }

    HTEventList_stopLoop();
    return HT_OK;
err:
    if (json != NULL) {
        json_decref(json);
    }
    HTRequest_delete(request);
    HTProfile_delete();
    exit(-1);
}

PRIVATE int step3_terminate_handler (HTRequest * request, HTResponse * response,
			       void * param, int status) 
{
    HTPrint("Step3 returned status %d\n", status);

    HTEventList_stopLoop();
    return HT_OK;
}

int main (int argc, char ** argv)
{
    HTRequest *request = NULL;
    char addr[256] = {0};
    char addr_base[128] = {0};
    char fn[128] = {0};
    char fn_with_slash[128] = {0};
    char *fn_ptr, *fn_ptr_previous;
    char *json_file_s;
    HTChunk *chunk;
    HTAssocList *formfields = NULL;
    HTAnchor *anchor = NULL;
    json_t *json_f;
    json_error_t error;

    if (argc < 2) {
        printf("Usage: %s <JSON log file>\n", argv[0]);
        return -1;
    }
    memcpy(fn_with_slash, argv[1], strlen(argv[1]));

    if (strstr(fn_with_slash, "/") == NULL) {
        memcpy(fn, fn_with_slash, strlen(fn_with_slash));
    } else {
        fn_ptr = fn_with_slash;
        while (1) {
            fn_ptr_previous = fn_ptr;
            fn_ptr = strstr(fn_ptr, "/");
            if (fn_ptr == NULL) {
                break;
            } else {
                fn_ptr++;
            }
        }
        memcpy(fn, fn_ptr_previous, strlen(fn_ptr_previous));
    }

    fn_ptr = strstr(fn, ".json");
    if (fn_ptr != NULL) {
        fn_ptr[0] = 0;
    }
    strcpy(addr, "http://test.qodome.com/api/v1/get_upload_params/?content_type=application/json&type=s&filename=");
    strcat(addr, fn);

    json_f = json_load_file(argv[1], 0, &error);
    if (!json_f) {
        printf("Failed to load file %s\n", argv[1]);
        return -2;
    }
    json_file_s = json_dumps(json_f, 0);
    if (json_file_s == NULL) {
        printf("Failed to dump json string\n");
        return -3;
    }

    HTProfile_newNoCacheClient("iQo", "1.0");

    HTPrint_setCallback(printer);
    HTTrace_setCallback(tracer);

    HTNet_addAfter(step1_terminate_handler, NULL, NULL, HT_ALL, HT_FILTER_LAST);

    // Step 1
    HTAlert_setInteractive(NO);

    request = HTRequest_new();

    HTRequest_addCredentials(request, "Authorization", "Token 6ee875a7584eaf9f84b7e33dbb7fc685ad2161a7");

    HTRequest_setOutputFormat(request, WWW_SOURCE);

    if ((chunk = HTLoadToChunk(addr, request)) == NULL) {
        return 0;
    }
    HTRequest_setContext(request, chunk);

    HTEventList_loop(request);

    HTRequest_delete(request);

    // Step 2
    /* Add our own filter to update the history list */
    HTNet_deleteAfter(step1_terminate_handler);
    HTNet_addAfter(step2_terminate_handler, NULL, NULL, HT_ALL, HT_FILTER_LAST);

    /* Set the timeout for long we are going to wait for a response */
    HTHost_setEventTimeout(20000);

    /* Create a list to hold the form arguments */
    formfields = HTAssocList_new();

    const char *key;
    json_t *value;

    json_object_foreach(json, key, value) {
        HTAssocList_addObject(formfields, "BOUNDARY", "Boundary+123456789");
        HTAssocList_addObject(formfields, key, json_string_value(value));
    }
    HTAssocList_addObject(formfields, "BOUNDARY", "Boundary+123456789");

    HTAssocList_addObject(formfields, "file", json_file_s);
    free(json_file_s);
    json_file_s = NULL;
    HTAssocList_addObject(formfields, "BOUNDARY", "Boundary+123456789--");

	/* Create a request */
	request = HTRequest_new();

	/* Set the default output to "asis" */
	HTRequest_setOutputFormat(request, WWW_SOURCE);

	/* Get an anchor object for the URI */
	anchor = HTAnchor_findAddress("http://test.media.qodome.com");

	HTPostFormAnchorToChunk(formfields, anchor, request, 1);
	HTAssocList_delete(formfields);

	HTEventList_loop(request);

    HTRequest_delete(request);

    // Step 3
    HTNet_deleteAfter(step2_terminate_handler);
    HTNet_addAfter(step3_terminate_handler, NULL, NULL, HT_ALL, HT_FILTER_LAST);
    strcpy(addr_base, "http://test.qodome.com/api/v1/healths/");

    HTHost_setEventTimeout(20000);

    formfields = HTAssocList_new();

    HTAssocList_addObject(formfields, "type", "s");
    HTAssocList_addObject(formfields, "filename", json_string_value(json_fn));

	/* Create a request */
	request = HTRequest_new();
    HTRequest_addCredentials(request, "Authorization", "Token 6ee875a7584eaf9f84b7e33dbb7fc685ad2161a7");

	/* Set the default output to "asis" */
	HTRequest_setOutputFormat(request, WWW_SOURCE);

	/* Get an anchor object for the URI */
	anchor = HTAnchor_findAddress(addr_base);

	HTPostFormAnchorToChunk(formfields, anchor, request, 0);
	HTAssocList_delete(formfields);

	HTEventList_loop(request);

    HTRequest_delete(request);

    HTProfile_delete();

    return 0;
}
