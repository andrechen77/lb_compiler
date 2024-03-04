#include "parser.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>

namespace pegtl = TAO_PEGTL_NAMESPACE;

namespace Lb::parser {

    struct ParseNode {
		// members
		Vec<Uptr<ParseNode>> children;
		pegtl::internal::inputerator begin;
		pegtl::internal::inputerator end;
		const std::type_info *rule; // which rule this node matched on
		std::string_view type;// only used for displaying parse tree

		// special methods
		ParseNode() = default;
		ParseNode(const ParseNode &) = delete;
		ParseNode(ParseNode &&) = delete;
		ParseNode &operator=(const ParseNode &) = delete;
		ParseNode &operator=(ParseNode &&) = delete;
		~ParseNode() = default;

		// methods used for parsing

		template<typename Rule, typename ParseInput, typename... States>
		void start(const ParseInput &in, States &&...) {
			this->begin = in.inputerator();
			std::string_view strview {
				this->begin.data,
				10
			};
			std::cout << "Starting to parse " << pegtl::demangle<Rule>() << " " << in.position() << ": \"" << strview << "\"\n";
		}

		template<typename Rule, typename ParseInput, typename... States>
		void success(const ParseInput &in, States &&...) {
			this->end = in.inputerator();
			this->rule = &typeid(Rule);
			this->type = pegtl::demangle<Rule>();
			this->type.remove_prefix(this->type.find_last_of(':') + 1);
			std::cout << "Successfully parsed " << pegtl::demangle<Rule>() << " at " << in.position() << ": \"" << this->string_view() << "\"\n";
		}

		template<typename Rule, typename ParseInput, typename... States>
		void failure(const ParseInput &in, States &&...) {}

		template<typename... States>
		void emplace_back(Uptr<ParseNode> &&child, States &&...) {
			children.emplace_back(mv(child));
		}

		std::string_view string_view() const {
			return {
				this->begin.data,
				static_cast<std::size_t>(this->end.data - this->begin.data)
			};
		}

		const ParseNode &operator[](int index) const {
			return *this->children.at(index);
		}

		// methods used to display the parse tree

		bool has_content() const noexcept {
			return this->end.data != nullptr;
		}

		bool is_root() const noexcept {
			return static_cast<bool>(this->rule);
		}
	};
    namespace rules {
        using namespace pegtl;
        template<typename Result, typename Separator, typename...Rules>
		struct interleaved_impl;
		template<typename... Results, typename Separator, typename Rule0, typename... RulesRest>
		struct interleaved_impl<seq<Results...>, Separator, Rule0, RulesRest...> :
			interleaved_impl<seq<Results..., Rule0, Separator>, Separator, RulesRest...>
		{};
		template<typename... Results, typename Separator, typename Rule0>
		struct interleaved_impl<seq<Results...>, Separator, Rule0> {
			using type = seq<Results..., Rule0>;
		};
		template<typename Separator, typename... Rules>
		using interleaved = typename interleaved_impl<seq<>, Separator, Rules...>::type;
        
        struct CommentRule :
			disable<
				TAO_PEGTL_STRING("//"),
				until<eolf>
			>
		{};

		struct SpaceRule :
			sor<one<' '>, one<'\t'>>
		{};

		struct SpacesRule :
			star<SpaceRule>
		{};

		struct LineSeparatorsRule :
			star<seq<SpacesRule, eol>>
		{};

		struct LineSeparatorsWithCommentsRule :
			star<
				seq<
					SpacesRule,
					sor<eol, CommentRule>
				>
			>
		{};

		struct SpacesOrNewLines :
			star<sor<SpaceRule, eol>>
		{};

		struct NameRule :
			ascii::identifier 
		{};

        struct LabelRule :
			seq<one<':'>, NameRule>
		{};

        struct NumberRule :
			sor<
				seq<
					opt<sor<one<'-'>, one<'+'>>>,
					range<'1', '9'>,
					star<digit>
				>,
				one<'0'>
			>
		{};
        struct ComparisonRule :
            sor<
                TAO_PEGTL_STRING("<="),
				TAO_PEGTL_STRING(">="),
				TAO_PEGTL_STRING("="),
				TAO_PEGTL_STRING("<"),
				TAO_PEGTL_STRING(">")
            >
        {};

        struct OperatorRule :
			sor<
				TAO_PEGTL_STRING("<<"),
				TAO_PEGTL_STRING(">>"),
				TAO_PEGTL_STRING("+"),
				TAO_PEGTL_STRING("-"),
				TAO_PEGTL_STRING("*"),
				TAO_PEGTL_STRING("&"),
                ComparisonRule
			>
		{};

        struct InexplicableTRule :
			sor<
				NameRule,
				NumberRule
			>
		{};

        struct TypeRule :
			sor<
				seq<
					TAO_PEGTL_STRING("int64"),
					star<
						TAO_PEGTL_STRING("[]")
					>
				>,
				TAO_PEGTL_STRING("tuple"),
				TAO_PEGTL_STRING("code")
			>
		{};

        struct VoidableTypeRule :
			sor<
				TypeRule,
				TAO_PEGTL_STRING("void")
			>
		{};

        struct ArgsRule :
			opt<list<
				InexplicableTRule,
				one<','>,
				SpaceRule
			>>
		{};

        struct NamesRule :
            seq<
                SpacesRule,
                NameRule,
                star<
                    interleaved<
                        SpacesRule,
                        one<','>,
                        NameRule
                    >
                >
            >
        {};

        struct ArrowRule : TAO_PEGTL_STRING("\x3c-") {};

        struct ConditionRule :
            interleaved<
                SpacesRule,
                InexplicableTRule,
                ComparisonRule,
                InexplicableTRule
            >
        {};

        struct ArrayAccess :
			plus<
				interleaved<
					SpacesRule,
					one<'['>,
					InexplicableTRule,
					one<']'>
				>
			>
		{};

        struct InstructionTypeDeclarationRule :
            interleaved<
				SpacesRule,
				VoidableTypeRule,
				NamesRule 
			>
        {};

        struct InstructionPureAssignmentRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				InexplicableTRule 
			>
		{};

        struct InstructionOperatorAssignmentRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				InexplicableTRule,
				OperatorRule,
				InexplicableTRule
			>
		{};

        struct InstructionLabelRule :
            seq<
                SpacesRule,
                LabelRule
            >
        {};

        struct InstructionIfStatementRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("if"),
                one<'('>,
                ConditionRule,
                one<')'>,
                LabelRule,
                LabelRule
            >
        {};

        struct InstructionGotoRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("goto"),
                LabelRule
            >
        {};

        struct InstructionReturnRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("return"),
                opt<InexplicableTRule>
            >
        {};

        struct InstructionWhileStatementRule :
            interleaved<
                SpacesRule,
                TAO_PEGTL_STRING("while"),
                one<'('>,
                ConditionRule,
                one<')'>,
                LabelRule,
                LabelRule
            >
        {};

        struct InstructionContinueRule :
            seq<
                SpacesRule,
                TAO_PEGTL_STRING("continue")
            >
        {};

        struct InstructionBreakRule :
            seq<
                SpacesRule,
                TAO_PEGTL_STRING("break")
            >
        {};

        struct InstructionArrayLoadRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				NameRule,
				ArrayAccess
			>
		{};

        struct InstructionArrayStoreRule : 
			interleaved<
				SpacesRule,
				NameRule,
				ArrayAccess,
				ArrowRule,
				InexplicableTRule
			>
		{};

        struct InstructionLengthRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				TAO_PEGTL_STRING("length"),
				NameRule,
				opt<InexplicableTRule>
			>
		{};

        struct InstructionFunctionCallRule :
            interleaved<
                SpacesRule,
                NameRule,
                one<'('>,
                ArgsRule,
                one<')'>
            >
        {};

        struct InstructionScopeRule;

        struct InstructionFunctionCallAssignmentRule :
            interleaved<
                SpacesRule,
				NameRule,
				ArrowRule,
				NameRule,
				one<'('>,
				ArgsRule,
				one<')'>
            >
        {};

        struct InstructionArrayDeclarationRule :
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				TAO_PEGTL_STRING("new"),
				TAO_PEGTL_STRING("Array"),
				one<'('>,
				ArgsRule,
				one<')'>
			>
		{};
        
        struct InstructionTupleDeclarationRule : 
			interleaved<
				SpacesRule,
				NameRule,
				ArrowRule,
				TAO_PEGTL_STRING("new"),
				TAO_PEGTL_STRING("Tuple"),
				one<'('>,
				InexplicableTRule,
				one<')'>
			>
		{};

        struct InstructionRule : 
			sor<
				InstructionFunctionCallRule,
                InstructionFunctionCallAssignmentRule,
				InstructionTypeDeclarationRule,
				InstructionOperatorAssignmentRule,
                InstructionLabelRule,
                InstructionIfStatementRule,
                InstructionGotoRule,
                InstructionReturnRule,
                InstructionWhileStatementRule,
                InstructionContinueRule,
                InstructionBreakRule,
                InstructionArrayLoadRule,
                InstructionArrayStoreRule,
                InstructionLengthRule,
                InstructionArrayDeclarationRule,
                InstructionTupleDeclarationRule,
                InstructionScopeRule,
				InstructionPureAssignmentRule
			>
		{};

        struct InstructionScopeRule :
            interleaved<
                SpacesOrNewLines,
                one<'{'>,
                star<
                    seq<
                        LineSeparatorsWithCommentsRule,
						SpacesRule,
                        InstructionRule,
						LineSeparatorsWithCommentsRule
                    >
                >,
                one<'}'>
            >
        {};

        struct FunctionRule :
            interleaved<
                SpacesRule,
                VoidableTypeRule,
				NameRule,
                one<'('>,
                star<
                    interleaved<
                        SpacesRule,
                        TypeRule,
                        NameRule
                    >
                >,
                one<')'>,
                InstructionScopeRule
            >
        {};

        struct ProgramRule :
            seq<
				LineSeparatorsWithCommentsRule,
				SpacesRule,
				list<
					seq<
						SpacesRule,
						FunctionRule
					>,
					LineSeparatorsWithCommentsRule
				>,
				LineSeparatorsWithCommentsRule
			>
        {};

        template<typename Rule>
        struct Selector : pegtl::parse_tree::selector<
			Rule,
			pegtl::parse_tree::store_content::on<
				NameRule,
                LabelRule,
                NumberRule,
                ComparisonRule,
                OperatorRule,
                InexplicableTRule,
                TypeRule,
                VoidableTypeRule,
                ArgsRule,
                NamesRule,
                ConditionRule,
                ArrayAccess,
                InstructionTypeDeclarationRule,
                InstructionPureAssignmentRule,
                InstructionOperatorAssignmentRule,
                InstructionLabelRule,
                InstructionIfStatementRule,
                InstructionGotoRule,
                InstructionReturnRule,
                InstructionWhileStatementRule,
                InstructionContinueRule,
                InstructionBreakRule,
                InstructionArrayLoadRule,
                InstructionArrayStoreRule,
                InstructionLengthRule,
                InstructionFunctionCallRule,
                InstructionFunctionCallAssignmentRule,
                InstructionArrayDeclarationRule,
                InstructionTupleDeclarationRule,
                InstructionScopeRule,
				FunctionRule,
				ProgramRule
			>
		> 
        {};
    }
    void parse_file(char *fileName, Opt<std::string> parse_tree_output) {
        using EntryPointRule = pegtl::must<rules::ProgramRule>;
        if (pegtl::analyze<EntryPointRule>() != 0) {
			std::cerr << "There are problems with the grammar" << std::endl;
			exit(1);
		}
		pegtl::file_input<> fileInput(fileName);
		auto root = pegtl::parse_tree::parse<EntryPointRule, ParseNode, rules::Selector>(fileInput);
		if (!root) {
			std::cerr << "ERROR: Parser failed" << std::endl;
			exit(1);
		}
        if (parse_tree_output) {
			std::ofstream output_fstream(*parse_tree_output);
			if (output_fstream.is_open()) {
				pegtl::parse_tree::print_dot(output_fstream, *root);
				output_fstream.close();
			}
		}
		return;
    }
}