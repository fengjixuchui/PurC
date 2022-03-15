#include "purc.h"
#include "private/utils.h"
#include "private/debug.h"
#include "../helpers.h"

#include <gtest/gtest.h>


static const char *calculator_1 =
    "<!DOCTYPE hvml>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <title>计算器</title>"
    "        <link rel=\"stylesheet\" type=\"text/css\" href=\"calculator.css\" />"
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
    "    </head>"
    ""
    "    <body>"
    "        <div id=\"calculator\">"
    ""
    "            <div id=\"c_title\">"
    "                <h2>计算器</h2>"
    "            </div>"
    ""
    "            <div id=\"c_text\">"
    "                <input type=\"text\" id=\"text\" value=\"0\" readonly=\"readonly\" />"
    "            </div>"
    ""
    "            <div id=\"c_value\">"
    "                <archetype name=\"button\">"
    "                    <li class=\"$?.class\">$?.letters</li>"
    "                </archetype>"
    ""
    "                <ul>"
    "                    <iterate on=\"$buttons\">"
    "                        <update on=\"$@\" to=\"append\" with=\"$button\" />"
    "                        <except type=\"NoData\" raw>"
    "                            <p>Bad data!</p>"
    "                        </except>"
    "                    </iterate>"
    "                </ul>"
    "            </div>"
    "        </div>"
    "    </body>"
    ""
    "</hvml>";

static const char *calculator_2 =
    "<!DOCTYPE hvml>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <base href=\"$HVML.base(! 'https://gitlab.fmsoft.cn/hvml/hvml-docs/raw/master/samples/calculator/' )\" />"
    ""
    "<!--"
    "        <update on=\"$T.map\" from=\"assets/{$SYSTEM.locale}.json\" to=\"merge\" />"
    "-->"
    ""
    "        <update on=\"$T.map\" to=\"merge\">"
    "           {"
    "               \"HVML Calculator\": \"HVML 计算器\","
    "               \"Current Time: \": \"当前时间：\""
    "           }"
    "        </update>"
    ""
    "<!--"
    "        <init as=\"buttons\" from=\"assets/buttons.json\" />"
    "-->"
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
    "        <update on=\"$TIMERS\" to=\"displace\">"
    "            ["
    "                { \"id\" : \"clock\", \"interval\" : 1000, \"active\" : \"yes\" },"
    "            ]"
    "        </update>"
    ""
    "        <link rel=\"stylesheet\" type=\"text/css\" href=\"assets/calculator.css\" />"
    "    </head>"
    ""
    "    <body>"
    "        <div id=\"calculator\">"
    ""
    "            <div id=\"c_text\">"
    "                <input type=\"text\" id=\"text\" value=\"0\" readonly=\"readonly\" />"
    "            </div>"
    ""
    "            <div id=\"c_value\">"
    "                <archetype name=\"button\">"
    "                    <li class=\"$?.class\">$?.letters</li>"
    "                </archetype>"
    ""
    "                <ul>"
    "                    <iterate on=\"$buttons\">"
    "                        <update on=\"$@\" to=\"append\" with=\"$button\" />"
    "                        <except type=\"NoData\" raw>"
    "                            <p>Bad data!</p>"
    "                        </except>"
    "                    </iterate>"
    "                </ul>"
    "            </div>"

    "            <div id=\"c_title\">"
    "                <h2 id=\"c_title\">$T.get('HVML Calculator')"
    "                    <small>$T.get('Current Time: ')<span id=\"clock\">$SYSTEM.time('%H:%M:%S')</span></small>"
    "                </h2>"
    "                <observe on=\"$TIMERS\" for=\"expired:clock\">"
    "                    <update on=\"#clock\" at=\"textContent\" with=\"$SYSTEM.time('%H:%M:%S')\" />"
    "                    <update on=\"$TIMERS\" to=\"overwrite\">"
    "                       { \"id\" : \"clock\", \"active\" : \"no\" }"
    "                    </update>"
    "                    <forget on=\"$TIMERS\" for=\"expired:clock\"/>"
    "                </observe>"
    "            </div>"
    "        </div>"
    "    </body>"
    ""
    "</hvml>";

static const char *calculator_3 =
    "<!DOCTYPE hvml SYSTEM 'v: MATH'>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <base href=\"$HVML.base(! 'https://gitlab.fmsoft.cn/hvml/hvml-docs/raw/master/samples/calculator/' )\" />"
    ""
    "<!--"
    "        <update on=\"$T.map\" from=\"assets/{$SYSTEM.locale}.json\" to=\"merge\" />"
    "-->"
    ""
    "        <update on=\"$T.map\" to=\"merge\">"
    "           {"
    "               \"HVML Calculator\": \"HVML 计算器\","
    "               \"Current Time: \": \"当前时间：\""
    "           }"
    "        </update>"
    ""
    "<!--"
    "        <init as=\"buttons\" from=\"assets/buttons.json\" />"
    "-->"
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
    "        <update on=\"$TIMERS\" to=\"unite\">"
    "            ["
    "                { \"id\" : \"clock\", \"interval\" : 1000, \"active\" : \"yes\" },"
    "                { \"id\" : \"input\", \"interval\" : 1500, \"active\" : \"yes\" },"
    "            ]"
    "        </update>"
    ""
    "        <link rel=\"stylesheet\" type=\"text/css\" href=\"assets/calculator.css\" />"
    "    </head>"
    ""
    "    <body>"
    "        <div id=\"calculator\">"
    ""
    "            <div id=\"c_title\">"
    "                <h2 id=\"c_title\">$T.get('HVML Calculator')"
    "                    <small>$T.get('Current Time: ')<span id=\"clock\">$SYSTEM.time('%H:%M:%S')</span></small>"
    "                </h2>"
    "                <observe on=\"$TIMERS\" for=\"expired:clock\">"
    "                    <update on=\"#clock\" at=\"textContent\" with=\"$SYSTEM.time('%H:%M:%S')\" />"
    "                </observe>"
    "            </div>"
    ""
    "            <div id=\"c_text\">"
    "                <input type=\"text\" id=\"expression\" value=\"0\" readonly=\"readonly\" />"
    "                <observe on=\"$TIMERS\" for=\"expired:input\">"
    "                    <test on=\"$buttons[$SYSTEM.random($EJSON.count($buttons))].letters\">"
    "                        <match for=\"AS '='\" exclusively>"
    "                            <choose on=\"$MATH.eval($DOC.query('#expression').attr('value'))\">"
    "                                <update on=\"#expression\" at=\"attr.value\" with=\"$?\" />"
    "                                <update on=\"$TIMERS\" to=\"overwrite\">"
    "                                    { \"id\" : \"input\", \"active\" : \"no\" }"
    "                                </update>"
    "                                <catch for='*'>"
    "                                    <update on=\"#expression\" at=\"attr.value\" with=\"ERR\" />"
    "                                </catch>"
    "                            </choose>"
    "                        </match>"
    "                        <match for=\"AS 'C'\" exclusively>"
    "                            <update on=\"#expression\" at=\"attr.value\" with=\"\" />"
    "                        </match>"
    "                        <match for=\"AS '←'\" exclusively>"
    "                            <choose on=\"$DOC.query('#expression').attr.value\">"
    "                                <update on=\"#expression\" at=\"attr.value\" with=\"$STR.substr($?, 0, -1)\" />"
    "                            </choose>"
    "                        </match>"
    ""
    "                        <match>"
    "                            <update on=\"#expression\" at=\"attr.value\" with $= \"$?\" />"
    "                        </match>"
    ""
    "                        <match for=\"ANY\" exclusively>"
    "                            <update on=\"$TIMERS\" to=\"overwrite\">"
    "                                { \"id\" : \"input\", \"active\" : \"no\" }"
    "                            </update>"
    "                        </match>"
    "                    </test>"
    "                </observe>"
    "            </div>"
    ""
    "            <div id=\"c_value\">"
    "                <archetype name=\"button\">"
    "                    <li class=\"$?.class\">$?.letters</li>"
    "                </archetype>"
    ""
    "                <ul>"
    "                    <iterate on=\"$buttons\">"
    "                        <update on=\"$@\" to=\"append\" with=\"$button\" />"
    "                        <except type=\"NoData\" raw>"
    "                            <p>Bad data!</p>"
    "                        </except>"
    "                    </iterate>"
    "                </ul>"
    "            </div>"
    "        </div>"
    "    </body>"
    ""
    "</hvml>";

static const char *calculator_4 =
    "<!DOCTYPE hvml SYSTEM 'v: MATH'>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <base href=\"$HVML.base(! 'https://gitlab.fmsoft.cn/hvml/hvml-docs/raw/master/samples/calculator/' )\" />"
    ""
    "<!--"
    "        <update on=\"$T.map\" from=\"assets/{$SYSTEM.locale}.json\" to=\"merge\" />"
    "-->"
    ""
    "        <update on=\"$T.map\" to=\"merge\">"
    "           {"
    "               \"HVML Calculator\": \"HVML 计算器\","
    "               \"Current Time: \": \"当前时间：\""
    "           }"
    "        </update>"
    ""
    "<!--"
    "        <init as=\"buttons\" from=\"assets/buttons.json\" />"
    "-->"
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
    "<!--"
    "        <init as=\"expressions\" from=\"assets/expressions.json\" />"
    "-->"
    ""
    "        <init as=\"expressions\">"
    "           ["
    "               \"7*3=\","
    "           ]"
    "        </init>"
    ""
    "        <title>$T.get('HVML Calculator')</title>"
    ""
    "        <update on=\"$TIMERS\" to=\"unite\">"
    "            ["
    "                { \"id\" : \"clock\", \"interval\" : 1000, \"active\" : \"yes\" },"
    "                { \"id\" : \"input\", \"interval\" : 1500, \"active\" : \"yes\" },"
    "            ]"
    "        </update>"
    ""
    "        <link rel=\"stylesheet\" type=\"text/css\" href=\"assets/calculator.css\" />"
    "    </head>"
    ""
    "    <body>"
    "        <init as=\"exp_chars\" with=\"[]\" />"
    ""
    "        <iterate on=\"$expressions\" by=\"RANGE: FROM 0\" >"
    "            <update on=\"$exp_chars\" to=\"append\" with=\"[]\" />"
    ""
    "            <iterate on=\"$?\" by=\"CHAR: FROM 0\" >"
    "                <update on=\"$exp_chars\" at=\"$1%\" to=\"append\" with=\"$?\" />"
    "            </iterate>"
    "        </iterate>"
    ""
    "        <init as=\"info\">"
    "            {"
    "                \"chars\" : $exp_chars[$SYSTEM.random($EJSON.count($exp_chars))],"
    "                \"index\" : 0,"
    "            }"
    "        </init>"
    ""
    "        <div id=\"calculator\">"
    ""
    "            <div id=\"c_title\">"
    "                <h2 id=\"c_title\">$T.get('HVML Calculator')"
    "                    <small>$T.get('Current Time: ')<span id=\"clock\">$SYSTEM.time('%H:%M:%S')</span></small>"
    "                </h2>"
    "                <observe on=\"$TIMERS\" for=\"expired:clock\">"
    "                    <update on=\"#clock\" at=\"textContent\" to=\"displace\" with=\"$SYSTEM.time('%H:%M:%S')\" />"
    "                </observe>"
    "            </div>"
    ""
    "            <div id=\"c_text\">"
    "                <input type=\"text\" id=\"expression\" value=\"\" readonly=\"readonly\" />"
    "                <observe on=\"$TIMERS\" for=\"expired:input\">"
    "                    <test on=\"$info.chars[$info.index]\">"
    "                        <update on=\"$info\" at=\".index\" to=\"displace\" with=\"$MATH.add($info.index, 1)\" />"
    ""
    "                        <match for=\"AS '='\" exclusively>"
    "                            <choose on=\"$MATH.eval($DOC.query('#expression').attr('value'))\">"
    "                                <update on=\"#expression\" at=\"attr.value\" with=\"$?\" />"
    "                                <update on=\"$TIMERS\" to=\"overwrite\">"
    "                                    { \"id\" : \"input\", \"active\" : \"no\" }"
    "                                </update>"
    "                                <update on=\"$TIMERS\" to=\"overwrite\">"
    "                                    { \"id\" : \"clock\", \"active\" : \"no\" }"
    "                                </update>"
    "                                <catch for='*'>"
    "                                    <update on=\"#expression\" at=\"attr.value\" with=\"ERR\" />"
    "                                </catch>"
    "                                <update on=\"$TIMERS\" to=\"overwrite\">"
    "                                    { \"id\" : \"input\", \"active\" : \"no\" }"
    "                                </update>"
    "                                <update on=\"$TIMERS\" to=\"overwrite\">"
    "                                    { \"id\" : \"clock\", \"active\" : \"no\" }"
    "                                </update>"
    "                                <forget on=\"$TIMERS\" for=\"expired:clock\"/>"
    "                                <forget on=\"$TIMERS\" for=\"expired:input\"/>"
    "                            </choose>"
    "                        </match>"
    ""
    "                        <match for=\"AS 'C'\" exclusively>"
    "                            <update on=\"#expression\" at=\"attr.value\" with=\"\" />"
    "                        </match>"
    ""
    "                        <match for=\"AS '←'\" exclusively>"
    "                            <choose on=\"$DOC.query('#expression').attr.value\">"
    "                                <update on=\"#expression\" at=\"attr.value\" with=\"$STR.substr($?, 0, -1)\" />"
    "                            </choose>"
    "                        </match>"
    ""
    "                        <match>"
    "                            <update on=\"#expression\" at=\"attr.value\" with $= \"$?\" />"
    "                        </match>"
    "                    </test>"
    "                </observe>"
    "            </div>"
    ""
    "            <div id=\"c_value\">"
    "                <archetype name=\"button\">"
    "                    <li class=\"$?.class\">$?.letters</li>"
    "                </archetype>"
    ""
    "                <ul>"
    "                    <iterate on=\"$buttons\">"
    "                        <update on=\"$@\" to=\"append\" with=\"$button\" />"
    "                        <except type=\"NoData\" raw>"
    "                            <p>Bad data!</p>"
    "                        </except>"
    "                    </iterate>"
    "                </ul>"
    "            </div>"
    "        </div>"
    "    </body>"
    ""
    "</hvml>";

static const char *fibonacci_1 =
    "<!DOCTYPE hvml SYSTEM 'v: MATH'>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <title>Fibonacci Numbers</title>"
    "    </head>"
    ""
    "    <body>"
    "        <header>"
    "            <h1>Fibonacci Numbers less than 2000</h1>"
    "            <!--p hvml:raw>Using named array variable ($fibonacci), $MATH, and $EJSON</p-->"
    "        </header>"
    ""
    "        <init as=\"fibonacci\">"
    "            [0, 1, ]"
    "        </init>"
    ""
    "        <iterate on 1 by=\"ADD: LT 2000 BY $fibonacci[$MATH.sub($EJSON.count($fibonacci), 2)]\">"
    "            <update on=\"$fibonacci\" to=\"append\" with=\"$?\" />"
    "        </iterate>"
    ""
    "        <section>"
    "            <ol>"
    "                <archetype name=\"fibo-item\">"
    "                    <li>$?</li>"
    "                </archetype>"
    "                <iterate on=\"$fibonacci\">"
    "                   <update on=\"$@\" to=\"append\" with=\"$fibo-item\" />"
    "                </iterate>"
    "            </ol>"
    "        </section>"
    ""
    "        <footer>"
    "            <p>Totally $EJSON.count($fibonacci) numbers.</p>"
    "        </footer>"
    "    </body>"
    ""
    "</hvml>";

static const char *fibonacci_2 =
    "<!DOCTYPE hvml>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <title>Fibonacci Numbers</title>"
    "    </head>"
    ""
    "    <body>"
    "        <header>"
    "            <h1>Fibonacci Numbers less than 2000</h1>"
    "            <p hvml:raw>Using local array variable ($!) and negative index</p>"
    "        </header>"
    ""
    "        <init as='fibonacci' locally>"
    "            [0, 1, ]"
    "        </init>"
    ""
    "        <iterate on 1 by=\"ADD: LT 2000 BY $!.fibonacci[-2]\">"
    "            <update on=\"$1!.fibonacci\" to=\"append\" with=\"$?\" />"
    "        </iterate>"
    ""
    "        <section>"
    "            <ol>"
    "                <iterate on=\"$2!.fibonacci\">"
    "                    <li>$?</li>"
    "                </iterate>"
    "            </ol>"
    "        </section>"
    ""
    "        <footer>"
    "            <p>Totally $EJSON.count($1!.fibonacci) numbers.</p>"
    "        </footer>"
    "    </body>"
    ""
    "</hvml>";

static const char *fibonacci_3 =
    "<!DOCTYPE hvml>"
    "<hvml target=\"html\" lang=\"en\">"
    "    <head>"
    "        <title>Fibonacci Numbers</title>"
    "    </head>"
    ""
    "    <body>"
    "        <header>"
    "            <h1>Fibonacci Numbers less than 2000</h1>"
    "            <p hvml:raw>Using non-array local variables</p>"
    "        </header>"
    ""
    "        <init as=\"last_one\" with=0 locally />"
    "        <init as=\"last_two\" with=1 locally />"
    "        <init as=\"count\" with=2 locally />"
    ""
    "        <section>"
    "            <dl>"
    "                <iterate on 1 by=\"ADD: LT 2000 BY $2!.last_one\">"
    "                    <update on=\"$3!\" at=\".last_one\" to=\"displace\" with=\"$3!.last_two\" />"
    "                    <update on=\"$3!\" at=\".last_two\" to=\"displace\" with=\"$?\" />"
    "                    <update on=\"$3!\" at=\".count\" to=\"displace\" with += 1 />"
    "                    <dt>$%</dt>"
    "                    <dd>$?</dd>"
    "                </iterate>"
    "            </dl>"
    "        </section>"
    ""
    "        <footer>"
    "            <p>Totally $1!.count numbers.</p>"
    "        </footer>"
    "    </body>"
    ""
    "</hvml>";

TEST(interpreter, basic)
{
    (void)calculator_1;
    (void)calculator_2;
    (void)calculator_3;
    (void)calculator_4;
    (void)fibonacci_1;
    (void)fibonacci_2;
    (void)fibonacci_3;

    const char *hvmls[] = {
        calculator_1,
//        calculator_2,
//        calculator_3,
//        calculator_4,
//        fibonacci_1,
//        fibonacci_2,
//        fibonacci_3,
    };

    unsigned int modules = (PURC_MODULE_HVML | PURC_MODULE_PCRDR) & ~PURC_HAVE_FETCHER;

    PurCInstance purc(modules, "cn.fmsoft.hybridos.test", "test_attach_rdr",
            NULL);
    ASSERT_TRUE(purc);

    // get statitics information
    struct purc_variant_stat * stat = purc_variant_usage_stat ();
    ASSERT_NE(stat, nullptr);

    for (size_t i=0; i<PCA_TABLESIZE(hvmls); ++i) {
        const char *hvml = hvmls[i];
        purc_vdom_t vdom = purc_load_hvml_from_string(hvml);
        ASSERT_NE(vdom, nullptr);

        purc_renderer_extra_info extra_info = {};
        bool ret = purc_attach_vdom_to_renderer(vdom,
                "blank",           /* target_workspace */
                "blank",           /* target_window */
                NULL,               /* target_tabpage */
                NULL,               /* target_level */
                &extra_info);
        ASSERT_EQ(ret, true);
    }

    purc_run(PURC_VARIANT_INVALID, NULL);
}

