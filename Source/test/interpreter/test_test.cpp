/*
** Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>
**
** This file is a part of PurC (short for Purring Cat), an HVML interpreter.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#include "purc/purc.h"

#include <gtest/gtest.h>

TEST(observe, basic)
{
    const char *test_hvml =
    "<!DOCTYPE hvml SYSTEM 'v: MATH'>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <base href=\"$CRTN.base(! 'https://gitlab.fmsoft.cn/hvml/hvml-docs/raw/master/samples/calculator/' )\" />"
    ""
    ""
    "        <init as=\"buttons\" uniquely>"
    "            ["
    "                { \"letters\": \"7\", \"class\": \"number\" },"
    "                { \"letters\": \"8\", \"class\": \"number\" },"
    "                { \"letters\": \"9\", \"class\": \"number\" },"
    "                { \"letters\": \"←\", \"class\": \"c_blue backspace\" },"
    "                { \"letters\": \"C\", \"class\": \"c_blue clear\" },"
    "                { \"letters\": \"4\", \"class\": \"number\" },"
    "                { \"letters\": \"5\", \"class\": \"number\" },"
    "                { \"letters\": \"6\", \"class\": \"number\" },"
    "                { \"letters\": \"×\", \"class\": \"c_blue multiplication\" },"
    "                { \"letters\": \"÷\", \"class\": \"c_blue division\" },"
    "                { \"letters\": \"1\", \"class\": \"number\" },"
    "                { \"letters\": \"2\", \"class\": \"number\" },"
    "                { \"letters\": \"3\", \"class\": \"number\" },"
    "                { \"letters\": \"+\", \"class\": \"c_blue plus\" },"
    "                { \"letters\": \"-\", \"class\": \"c_blue subtraction\" },"
    "                { \"letters\": \"0\", \"class\": \"number\" },"
    "                { \"letters\": \"00\", \"class\": \"number\" },"
    "                { \"letters\": \".\", \"class\": \"number\" },"
    "                { \"letters\": \"%\", \"class\": \"c_blue percent\" },"
    "                { \"letters\": \"=\", \"class\": \"c_yellow equal\" },"
    "            ]"
    "        </init>"
    ""
    "        <title>$T.get('HVML Calculator')</title>"
    ""
    "        <link rel=\"stylesheet\" type=\"text/css\" href=\"assets/calculator.css\" />"
    "    </head>"
    ""
    "    <body>"
    "        <div id=\"calculator\">"
    "            <div id=\"c_query\">"
    "            </div>"
    "            <div>"
    "                test DOC.query(c_query).count() : $DOC.query(\"#c_query\").count() "
    "            </div>"
    "            <div>"
    "                test T.get result is : $T.get('HVML Calculator')"
    "            </div>"
    "            <div id=\"c_text\">"
    "                <test on=\"$buttons[$SYS.random($DATA.count($buttons))]\" by=\"KEY: ALL FOR KV\" in=\"#c_query\">"
    "                    <match for=\"AS 'C'\" exclusively>"
    "                    </match>"
    "                    <match for=\"AS 'C'\" excl>"
    "                    </match>"
    "                    <match for=\"AS 'C'\">"
    "                    </match>"
    "                </test>"
    "                <choose on=\"$buttons[$SYS.random($DATA.count($buttons))]\" by=\"KEY: ALL FOR KV\">"
    "                </choose>"
    "            </div>"
    "        </div>"
    "    </body>"
    ""
    "</hvml>"
    "";
    (void)test_hvml;

    const char *hvmls[] = {
        test_hvml,
    };

    purc_instance_extra_info info = {};
    int ret = 0;
    bool cleanup = false;

    // initial purc
    ret = purc_init_ex (PURC_MODULE_HVML, "cn.fmsoft.hybridos.test",
            "test_init", &info);

    ASSERT_EQ (ret, PURC_ERROR_OK);

    // get statitics information
    const struct purc_variant_stat * stat = purc_variant_usage_stat ();
    ASSERT_NE(stat, nullptr);

    for (size_t i=0; i<PCA_TABLESIZE(hvmls); ++i) {
        const char *hvml = hvmls[i];
        purc_vdom_t vdom = purc_load_hvml_from_string(hvml);
        purc_schedule_vdom_null(vdom);
        ASSERT_NE(vdom, nullptr);
    }

    purc_run(NULL);

    cleanup = purc_cleanup ();
    ASSERT_EQ (cleanup, true);
}

