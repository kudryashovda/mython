#include "runtime.h"
#include "test_runner_p.h"

#include <functional>

using namespace std;

namespace runtime {

    namespace {
        class Logger : public Object {
        public:
            static int instance_count;

            explicit Logger(int value_ = 0)
                : id_(value_) {
                ++instance_count;
            }

            Logger(const Logger& rhs)
                : id_(rhs.id_) {
                ++instance_count;
            }

            Logger(Logger&& rhs) noexcept
                : id_(rhs.id_) {
                ++instance_count;
            }

            Logger& operator=(const Logger& /*rhs*/) = default;
            Logger& operator=(Logger&& /*rhs*/) = default;

            [[nodiscard]] int GetId() const {
                return id_;
            }

            ~Logger() {
                --instance_count;
            }

            void Print(ostream& os, [[maybe_unused]] Context& context) override {
                os << id_;
            }

        private:
            int id_;
        };

        int Logger::instance_count = 0;

        void TestNumber() {
            Number num(127);

            DummyContext context;

            num.Print(context.output, context);
            ASSERT_EQUAL(context.output.str(), "127"s);
            ASSERT_EQUAL(num.GetValue(), 127);
        }

        void TestString() {
            String word("hello!"s);

            DummyContext context;
            word.Print(context.output, context);
            ASSERT_EQUAL(context.output.str(), "hello!"s);
            ASSERT_EQUAL(word.GetValue(), "hello!"s);
        }

        struct TestMethodBody : Executable {
            using Fn = std::function<ObjectHolder(Closure& closure, Context& context)>;
            Fn body;

            explicit TestMethodBody(Fn body)
                : body(std::move(body)) {
            }

            ObjectHolder Execute(Closure& closure, Context& context) override {
                if (body) {
                    return body(closure, context);
                }
                return {};
            }
        };

        void TestMethodInvocation() {
            DummyContext context;

            Closure base_closure;

            auto base_method_1 = [&base_closure, &context](Closure& closure, Context& ctx) {
                ASSERT_EQUAL(&context, &ctx);

                base_closure = closure;

                return ObjectHolder::Own(Number{ 123 });
            };

            auto base_method_2 = [&base_closure, &context](Closure& closure, Context& ctx) {
                ASSERT_EQUAL(&context, &ctx);
                base_closure = closure;
                return ObjectHolder::Own(Number{ 456 });
            };

            vector<Method> base_methods;

            base_methods.push_back(
                { "test"s, { "arg1"s, "arg2"s }, make_unique<TestMethodBody>(base_method_1) });

            base_methods.push_back({ "test_2"s, { "arg1"s }, make_unique<TestMethodBody>(base_method_2) });

            Class base_class{ "Base"s, std::move(base_methods), nullptr };

            ClassInstance base_inst{ base_class };

            base_inst.Fields()["base_field"s] = ObjectHolder::Own(String{ "hello"s });

            ASSERT(base_inst.HasMethod("test"s, 2U));

            auto res = base_inst.Call(
                "test"s, { ObjectHolder::Own(Number{ 1 }), ObjectHolder::Own(String{ "abc"s }) }, context);

            ASSERT(Equal(res, ObjectHolder::Own(Number{ 123 }), context));

            ASSERT_EQUAL(base_closure.size(), 3U);
            ASSERT_EQUAL(base_closure.count("self"s), 1U);
            ASSERT_EQUAL(base_closure.at("self"s).Get(), &base_inst);
            ASSERT_EQUAL(base_closure.count("self"s), 1U);
            ASSERT_EQUAL(base_closure.count("arg1"s), 1U);
            ASSERT(Equal(base_closure.at("arg1"s), ObjectHolder::Own(Number{ 1 }), context));
            ASSERT_EQUAL(base_closure.count("arg2"s), 1U);
            ASSERT(Equal(base_closure.at("arg2"s), ObjectHolder::Own(String{ "abc"s }), context));
            ASSERT_EQUAL(base_closure.count("base_field"s), 0U);

            Closure child_closure;

            auto child_method_1 = [&child_closure, &context](Closure& closure, Context& ctx) {
                ASSERT_EQUAL(&context, &ctx);

                child_closure = closure;

                return ObjectHolder::Own(String("child"s));
            };

            vector<Method> child_methods;

            child_methods.push_back(
                { "test"s, { "arg1_child"s, "arg2_child"s }, make_unique<TestMethodBody>(child_method_1) });

            Class child_class{ "Child"s, std::move(child_methods), &base_class };

            ClassInstance child_inst{ child_class };

            ASSERT(child_inst.HasMethod("test"s, 2U));

            base_closure.clear();

            res = child_inst.Call(
                "test"s, { ObjectHolder::Own(String{ "value1"s }), ObjectHolder::Own(String{ "value2"s }) },
                context);

            ASSERT(Equal(res, ObjectHolder::Own(String{ "child"s }), context));

            ASSERT(base_closure.empty());

            ASSERT_EQUAL(child_closure.size(), 3U);
            ASSERT_EQUAL(child_closure.count("self"s), 1U);
            ASSERT_EQUAL(child_closure.at("self"s).Get(), &child_inst);
            ASSERT_EQUAL(child_closure.count("arg1_child"s), 1U);
            ASSERT(Equal(child_closure.at("arg1_child"s), (ObjectHolder::Own(String{ "value1"s })), context));
            ASSERT_EQUAL(child_closure.count("arg2_child"s), 1U);

            ASSERT(Equal(child_closure.at("arg2_child"s), (ObjectHolder::Own(String{ "value2"s })), context));

            ASSERT(child_inst.HasMethod("test_2"s, 1U));

            child_closure.clear();

            res = child_inst.Call("test_2"s, { ObjectHolder::Own(String{ ":)"s }) }, context);

            ASSERT(Equal(res, ObjectHolder::Own(Number{ 456 }), context));

            ASSERT_EQUAL(base_closure.size(), 2U);
            ASSERT_EQUAL(base_closure.count("self"s), 1U);
            ASSERT_EQUAL(base_closure.at("self"s).Get(), &child_inst);
            ASSERT_EQUAL(base_closure.count("arg1"s), 1U);
            ASSERT(Equal(base_closure.at("arg1"s), (ObjectHolder::Own(String{ ":)"s })), context));

            ASSERT(!child_inst.HasMethod("test"s, 1U));

            ASSERT_THROWS(child_inst.Call("test"s, { ObjectHolder::None() }, context), runtime_error);
        }

        void TestNonowning() {
            ASSERT_EQUAL(Logger::instance_count, 0);
            Logger logger(784);
            {
                auto oh = ObjectHolder::Share(logger);
                ASSERT(oh);
            }
            ASSERT_EQUAL(Logger::instance_count, 1);

            auto oh = ObjectHolder::Share(logger);
            ASSERT(oh);
            ASSERT(oh.Get() == &logger);

            DummyContext context;
            oh->Print(context.output, context);

            ASSERT_EQUAL(context.output.str(), "784"sv);
        }

        void TestOwning() {
            ASSERT_EQUAL(Logger::instance_count, 0);
            {
                auto oh = ObjectHolder::Own(Logger());
                ASSERT(oh);
                ASSERT_EQUAL(Logger::instance_count, 1);
            }
            ASSERT_EQUAL(Logger::instance_count, 0);

            auto oh = ObjectHolder::Own(Logger(312));
            ASSERT(oh);
            ASSERT_EQUAL(Logger::instance_count, 1);

            DummyContext context;
            oh->Print(context.output, context);

            ASSERT_EQUAL(context.output.str(), "312"sv);
        }

        void TestMove() {
            {
                ASSERT_EQUAL(Logger::instance_count, 0);
                Logger logger;

                auto one = ObjectHolder::Share(logger);
                ObjectHolder two = std::move(one);

                ASSERT_EQUAL(Logger::instance_count, 1);
                ASSERT(two.Get() == &logger);
            }
            {
                ASSERT_EQUAL(Logger::instance_count, 0);
                auto one = ObjectHolder::Own(Logger());
                ASSERT_EQUAL(Logger::instance_count, 1);
                Object* stored = one.Get();
                ObjectHolder two = std::move(one);
                ASSERT_EQUAL(Logger::instance_count, 1);

                ASSERT(two.Get() == stored);
                ASSERT(!one);
            }
        }

        void TestNullptr() {
            ObjectHolder oh;
            ASSERT(!oh);
            ASSERT(!oh.Get());
        }

        void TestIsBool() {
            ASSERT(IsTrue(ObjectHolder::Own(Bool{ true })));
            ASSERT(!IsTrue(ObjectHolder::Own(Bool{ false })));

            ASSERT(!IsTrue(ObjectHolder::Own(String{ ""s })));
            ASSERT(IsTrue(ObjectHolder::Own(String{ "abc"s })));

            ASSERT(IsTrue(ObjectHolder::Own(Number{ 1 })));
            ASSERT(!IsTrue(ObjectHolder::Own(Number{ 0 })));
        }

        void TestPrint() {

            {
                DummyContext ctx;
                auto string_obj = ObjectHolder::Own(String{ "abc"s });
                string_obj.Get()->Print(ctx.output, ctx);

                ASSERT(ctx.output.str() == "abc"s);
            }
            {
                DummyContext ctx;
                auto number_obj = ObjectHolder::Own(Number{ 1234 });
                number_obj.Get()->Print(ctx.output, ctx);
                ASSERT(ctx.output.str() == "1234"s)
            }
            {
                DummyContext ctx;
                auto bool_obj = ObjectHolder::Own(Bool{ true });
                bool_obj.Get()->Print(ctx.output, ctx);
                ASSERT(ctx.output.str() == "True"s)
            }
            {
                DummyContext context;

                Closure base_closure;

                auto base_method_1 = [&base_closure, &context](Closure& closure, Context& ctx) {
                    return ObjectHolder::Own(Number{ 123 });
                };

                auto base_method_2 = [&base_closure, &context](Closure& closure, Context& ctx) {
                    return ObjectHolder::Own(Number{ 456 });
                };

                vector<Method> base_methods;

                base_methods.push_back(
                    { "test"s, { "arg1"s, "arg2"s }, make_unique<TestMethodBody>(base_method_1) });

                base_methods.push_back({ "test_2"s, { "arg1"s }, make_unique<TestMethodBody>(base_method_2) });

                Class base_class{ "Base"s, std::move(base_methods), nullptr };

                ClassInstance base_inst{ base_class };

                base_inst.Fields()["base_field"s] = ObjectHolder::Own(String{ "hello"s });

                base_class.Print(context.output, context);
                ASSERT(context.output.str() == "Class Base"s)

                context.output.str(""s);
                base_inst.Print(context.output, context);

                auto str = context.output.str();

                stringstream ss;
                ss << &base_inst;

                ASSERT(context.output.str() == ss.str());
            }
            {
                DummyContext context;

                Closure base_closure;

                auto base_method_1 = [&base_closure, &context](Closure& closure, Context& ctx) {
                    return ObjectHolder::Own(String{ "string string string"s });
                };

                auto base_method_2 = [&base_closure, &context](Closure& closure, Context& ctx) {
                    return ObjectHolder::Own(Number{ 456 });
                };

                vector<Method> base_methods;

                base_methods.push_back(
                    { "__str__"s, {}, make_unique<TestMethodBody>(base_method_1) });

                base_methods.push_back({ "test_2"s, { "arg1"s }, make_unique<TestMethodBody>(base_method_2) });

                Class base_class{ "Base"s, std::move(base_methods), nullptr };

                ClassInstance base_inst{ base_class };

                base_inst.Fields()["base_field"s] = ObjectHolder::Own(String{ "hello"s });

                base_class.Print(context.output, context);
                ASSERT(context.output.str() == "Class Base"s)

                context.output.str(""s);
                base_inst.Print(context.output, context);

                ASSERT(context.output.str() == "string string string"s);
            }
        }

        void TestCall() {
            DummyContext context;

            Closure base_closure;

            auto base_method_1 = [](Closure& closure, Context& ctx) {
                auto x = closure.at("arg1"s);

                auto f = closure.at("self"s);

                return ObjectHolder::Own(Number{ 123 });
            };

            vector<Method> base_methods;

            base_methods.push_back(
                { "test"s, { "arg1"s }, make_unique<TestMethodBody>(base_method_1) });

            Class base_class{ "Base"s, std::move(base_methods), nullptr };

            ClassInstance base_inst{ base_class };

            base_inst.Fields()["base_field"s] = ObjectHolder::Own(Number{ 6 });

            auto res = base_inst.Call(
                "test"s, { ObjectHolder::Own(Number{ 5 }) }, context);

            res.Get()->Print(context.output, context);

            ASSERT(context.output.str() == "123"s);
        }

        void TestClassEq() {
            DummyContext context;

            auto class1_method = [](Closure& closure, Context& ctx) {
                auto self_obj = closure.at("self"s);

                auto l_ptr_class_inst = self_obj.TryAs<runtime::ClassInstance>();
                const auto lvalue = l_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                auto other_obj = closure.at("other"s);
                auto r_ptr_class_inst = other_obj.TryAs<runtime::ClassInstance>();
                const auto rvalue = r_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                return ObjectHolder::Own(Bool{ lvalue == rvalue });
            };

            vector<Method> class1_methods;
            class1_methods.push_back(
                { "__eq__"s, { "other"s }, make_unique<TestMethodBody>(class1_method) });
            Class class1{ "Class1"s, std::move(class1_methods), nullptr };
            ClassInstance class1_inst{ class1 };
            class1_inst.Fields()["value"s] = ObjectHolder::Own(Number{ 6 });

            auto class2_method = [](Closure& closure, Context& ctx) {
                auto self_obj = closure.at("self"s);

                auto l_ptr_class_inst = self_obj.TryAs<runtime::ClassInstance>();
                const auto lvalue = l_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                auto other_obj = closure.at("other"s);
                auto r_ptr_class_inst = other_obj.TryAs<runtime::ClassInstance>();
                const auto rvalue = r_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                return ObjectHolder::Own(Bool{ lvalue == rvalue });
            };

            vector<Method> class2_methods;
            class2_methods.push_back(
                { "__eq__"s, { "other"s }, make_unique<TestMethodBody>(class1_method) });
            Class class2{ "Class2"s, std::move(class2_methods), nullptr };
            ClassInstance class2_inst{ class2 };
            class2_inst.Fields()["value"s] = ObjectHolder::Own(Number{ 7 });

            ASSERT(!Equal(ObjectHolder::Own(std::move(class1_inst)), ObjectHolder::Own(std::move(class2_inst)), context));
        }

        void TestLess() {
            {
                DummyContext context;
                ASSERT(!Less(ObjectHolder::Own(String{ "caa"s }), ObjectHolder::Own(String{ "bbb"s }), context));
                ASSERT(!Less(ObjectHolder::Own(Number{ 7 }), ObjectHolder::Own(Number{ 3 }), context));
                ASSERT(Less(ObjectHolder::Own(Bool{ false }), ObjectHolder::Own(Bool{ true }), context));
            }

            {
                DummyContext context;

                auto class1_method = [](Closure& closure, Context& ctx) {
                    auto self_obj = closure.at("self"s);

                    auto l_ptr_class_inst = self_obj.TryAs<runtime::ClassInstance>();
                    const auto lvalue = l_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                    auto other_obj = closure.at("other"s);
                    auto r_ptr_class_inst = other_obj.TryAs<runtime::ClassInstance>();
                    const auto rvalue = r_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                    return ObjectHolder::Own(Bool{ lvalue < rvalue });
                };

                vector<Method> class1_methods;
                class1_methods.push_back(
                    { "__lt__"s, { "other"s }, make_unique<TestMethodBody>(class1_method) });
                Class class1{ "Class1"s, std::move(class1_methods), nullptr };
                ClassInstance class1_inst{ class1 };
                class1_inst.Fields()["value"s] = ObjectHolder::Own(Number{ 6 });

                auto class2_method = [](Closure& closure, Context& ctx) {
                    auto self_obj = closure.at("self"s);

                    auto l_ptr_class_inst = self_obj.TryAs<runtime::ClassInstance>();
                    const auto lvalue = l_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                    auto other_obj = closure.at("other"s);
                    auto r_ptr_class_inst = other_obj.TryAs<runtime::ClassInstance>();
                    const auto rvalue = r_ptr_class_inst->Fields().at("value").TryAs<Number>()->GetValue();

                    return ObjectHolder::Own(Bool{ lvalue < rvalue });
                };

                vector<Method> class2_methods;
                class2_methods.push_back(
                    { "__lt__"s, { "other"s }, make_unique<TestMethodBody>(class1_method) });
                Class class2{ "Class2"s, std::move(class2_methods), nullptr };
                ClassInstance class2_inst{ class2 };
                class2_inst.Fields()["value"s] = ObjectHolder::Own(Number{ 7 });

                ASSERT(Less(ObjectHolder::Own(std::move(class1_inst)), ObjectHolder::Own(std::move(class2_inst)), context));
            }
        }

    } // namespace

    void RunObjectsTests(TestRunner& tr) {
        RUN_TEST(tr, runtime::TestNumber);
        RUN_TEST(tr, runtime::TestString);
        RUN_TEST(tr, runtime::TestMethodInvocation);
    }

    void RunObjectHolderTests(TestRunner& tr) {
        RUN_TEST(tr, runtime::TestNonowning);
        RUN_TEST(tr, runtime::TestOwning);
        RUN_TEST(tr, runtime::TestMove);
        RUN_TEST(tr, runtime::TestNullptr);
    }

    void RunUserTests(TestRunner& tr) {
        RUN_TEST(tr, runtime::TestIsBool);
        RUN_TEST(tr, runtime::TestPrint);
        RUN_TEST(tr, runtime::TestCall);
        RUN_TEST(tr, runtime::TestClassEq);
        RUN_TEST(tr, runtime::TestLess);
    }

} // namespace runtime