#include "purc.h"
#include "private/avl.h"
#include "private/hashtable.h"
#include "private/variant.h"
#include "private/errors.h"
#include "private/debug.h"
#include "private/utils.h"
#include "private/dvobjs.h"

#include "../helpers.h"

#include <stdio.h>
#include <errno.h>
#include <gtest/gtest.h>

extern purc_variant_t get_variant (char *buf, size_t *length);
extern void get_variant_total_info (size_t *mem, size_t *value, size_t *resv);
#define MAX_PARAM_NR    20

TEST(dvobjs, dvobjs_string_contains)
{
    const char *function[] = {"contains", "ends_with"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_BOOLEAN), true);
                        ASSERT_EQ(ret_var->b, ret_result->b);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}


TEST(dvobjs, dvobjs_string_explode)
{
    const char *function[] = {"explode"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_ARRAY), true);
                        size_t number = purc_variant_array_get_size (ret_var);
                        size_t i = 0;

                        ASSERT_EQ(number, purc_variant_array_get_size (
                                    ret_result));
                        for (i = 0; i < number; i++) {
                            purc_variant_t v1 = purc_variant_array_get (
                                    ret_var, i);
                            purc_variant_t v2 = purc_variant_array_get (
                                    ret_result, i);

                            const char *s1 = purc_variant_get_string_const (v1);
                            const char *s2 = purc_variant_get_string_const (v2);
                            ASSERT_STREQ (s1, s2);
                        }
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}


TEST(dvobjs, dvobjs_string_shuffle)
{
    const char *function[] = {"shuffle"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);
                        ASSERT_EQ(purc_variant_is_type (param[0],
                                    PURC_VARIANT_TYPE_STRING), true);
                        size_t number1 = purc_variant_string_length (ret_var);
                        size_t number2 = purc_variant_string_length (param[0]);
                        ASSERT_EQ(number1, number2);

                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                param[0]);
                        size_t i = 0;
                        unsigned int v1 = 0;
                        unsigned int v2 = 0;

                        for (i = 0; i < number1; i++) {
                           v1 += *(s1 + i);
                           v2 += *(s2 + i);
                        }
                        ASSERT_EQ(v1, v2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}

TEST(dvobjs, dvobjs_string_replace)
{
    const char *function[] = {"replace"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);

                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                ret_result);
                        ASSERT_STREQ (s1, s2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}


TEST(dvobjs, dvobjs_string_format_c)
{
    const char *function[] = {"format_c"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);

                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                ret_result);
                        ASSERT_STREQ (s1, s2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}

TEST(dvobjs, dvobjs_string_format_p)
{
    const char *function[] = {"format_p"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);

                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                ret_result);
                        ASSERT_STREQ (s1, s2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}

TEST(dvobjs, dvobjs_string_strcat)
{
    const char *function[] = {"strcat"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);

                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                ret_result);
                        ASSERT_STREQ (s1, s2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}

TEST(dvobjs, dvobjs_string_lower)
{
    const char *function[] = {"lower"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);

                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                ret_result);
                        ASSERT_STREQ (s1, s2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}

TEST(dvobjs, dvobjs_string_upper)
{
    const char *function[] = {"upper"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);

                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                ret_result);
                        ASSERT_STREQ (s1, s2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}

TEST(dvobjs, dvobjs_string_strlen)
{
    const char *function[] = {"strlen"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_ULONGINT), true);
                        uint64_t src;
                        uint64_t desc;
                        purc_variant_cast_to_ulongint (ret_var, &desc, false);
                        purc_variant_cast_to_ulongint (ret_result, &src, false);

                        ASSERT_EQ (src, desc);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}

TEST(dvobjs, dvobjs_string_implode)
{
    const char *function[] = {"implode"};
    purc_variant_t param[MAX_PARAM_NR];
    purc_variant_t ret_var = PURC_VARIANT_INVALID;
    purc_variant_t ret_result = PURC_VARIANT_INVALID;
    size_t function_size = PCA_TABLESIZE(function);
    size_t i = 0;
    size_t line_number = 0;
    size_t sz_total_mem_before = 0;
    size_t sz_total_values_before = 0;
    size_t nr_reserved_before = 0;
    size_t sz_total_mem_after = 0;
    size_t sz_total_values_after = 0;
    size_t nr_reserved_after = 0;
    char file_path[1024];
    char data_path[PATH_MAX+1];
    const char *env = "DVOBJS_TEST_PATH";
    test_getpath_from_env_or_rel(data_path, sizeof(data_path),
        env, "test_files");
    std::cerr << "env: " << env << "=" << data_path << std::endl;

    // get and function
    purc_instance_extra_info info = {};
    int ret = purc_init ("cn.fmsoft.hybridos.test", "test_init", &info);
    ASSERT_EQ (ret, PURC_ERROR_OK);

    purc_variant_t string = pcdvobjs_get_string();
    ASSERT_NE(string, nullptr);
    ASSERT_EQ(purc_variant_is_object (string), true);

    for (i = 0; i < function_size; i++) {
        printf ("test _STR.%s:\n", function[i]);

        purc_variant_t dynamic = purc_variant_object_get_by_ckey (string,
                function[i]);
        ASSERT_NE(dynamic, nullptr);
        ASSERT_EQ(purc_variant_is_dynamic (dynamic), true);

        purc_dvariant_method func = NULL;
        func = purc_variant_dynamic_get_getter (dynamic);
        ASSERT_NE(func, nullptr);

        // get test file
        strcpy (file_path, data_path);
        strcat (file_path, "/");
        strcat (file_path, function[i]);
        strcat (file_path, ".test");

        FILE *fp = fopen(file_path, "r");   // open test_list
        ASSERT_NE(fp, nullptr);

        char *line = NULL;
        size_t sz = 0;
        ssize_t read = 0;
        size_t j = 0;
        size_t length_sub = 0;

        line_number = 0;

        get_variant_total_info (&sz_total_mem_before, &sz_total_values_before,
                &nr_reserved_before);

        while ((read = getline(&line, &sz, fp)) != -1) {
            *(line + read - 1) = 0;
            line_number ++;

            if (strncasecmp (line, "test_begin", 10) == 0) {
                printf ("\ttest case on line %ld\n", line_number);

                // get parameters
                read = getline(&line, &sz, fp);
                *(line + read - 1) = 0;
                line_number ++;

                if (strcmp (line, "param_begin") == 0) {
                    j = 0;

                    // get param
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "param_end") == 0) {
                            break;
                        }
                        param[j] = get_variant (line, &length_sub);
                        j++;
                        ASSERT_LE(j, MAX_PARAM_NR);
                    }

                    // get result
                    read = getline(&line, &sz, fp);
                    *(line + read - 1) = 0;
                    line_number ++;

                    ret_result = get_variant(line, &length_sub);

                    // test case end
                    while (1) {
                        read = getline(&line, &sz, fp);
                        *(line + read - 1) = 0;
                        line_number ++;

                        if (strcmp (line, "test_end") == 0) {
                            break;
                        }
                    }

                    ret_var = func (NULL, j, param);

                    if (ret_result == PURC_VARIANT_INVALID) {
                        ASSERT_EQ(ret_var, PURC_VARIANT_INVALID);
                    } else {
                        // USER MODIFIED HERE.
                        ASSERT_EQ(purc_variant_is_type (ret_var,
                                    PURC_VARIANT_TYPE_STRING), true);
                        const char *s1 = purc_variant_get_string_const (
                                ret_var);
                        const char *s2 = purc_variant_get_string_const (
                                ret_result);
                        ASSERT_STREQ (s1, s2);
                    }
                    if (ret_var != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_var);
                        ret_var = PURC_VARIANT_INVALID;
                    }

                    if (ret_result != PURC_VARIANT_INVALID) {
                        purc_variant_unref(ret_result);
                        ret_result = PURC_VARIANT_INVALID;
                    }

                    for (size_t i = 0; i < j; ++i) {
                        if (param[i] != PURC_VARIANT_INVALID) {
                            purc_variant_unref(param[i]);
                            param[i] = PURC_VARIANT_INVALID;
                        }
                    }

                    get_variant_total_info (&sz_total_mem_after,
                            &sz_total_values_after, &nr_reserved_after);
                    ASSERT_EQ(sz_total_values_before, sz_total_values_after);
                    ASSERT_EQ(sz_total_mem_after,
                            sz_total_mem_before + (nr_reserved_after -
                                nr_reserved_before) * sizeof(purc_variant));
                } else
                    continue;
            } else
                continue;
        }

        length_sub++;
        fclose(fp);
        if (line)
            free(line);
    }

    purc_variant_unref(string);
    purc_cleanup ();
}
