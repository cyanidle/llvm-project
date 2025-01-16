// Declares clang::SyntaxOnlyAction.
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CommandLine.h"


#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

using std::string;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("add-describe options");


static cl::opt<bool> DryRun("dry-run", 
  cl::init(false), 
  cl::desc("Do not modify files: print result to stdout"), 
  cl::cat(MyToolCategory)
);
// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nTBD...\n");

static DeclarationMatcher PureMethodMatcher = 
    cxxMethodDecl(isPure()).bind("method");


static void checkRecord(const clang::CXXRecordDecl* record, SourceManager& manager) {
    if (!record->isInterfaceLike()) {
      // TODO: check why fails
      // auto& Diag = manager.getDiagnostics();
      // unsigned NonIface = Diag.getCustomDiagID(DiagnosticsEngine::Error, "class is not an interface %0");
      // string buff;
      // llvm::raw_string_ostream stream(buff);
      // record->printName(stream);
      // Diag.Report(NonIface) << buff;
    }
}

class AddDescribe : public MatchFinder::MatchCallback {
public :

  

  virtual void run(const MatchFinder::MatchResult &Result) override {
    src = Result.SourceManager;
    rewriter.setSourceMgr(*Result.SourceManager, Result.Context->getLangOpts());
    if (const clang::CXXMethodDecl* method = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("method")) {
      auto parent = method->getParent();
      if (currentParent != parent) {
        checkRecord(parent, *src);
        if (!parent->isInterfaceLike()) {
          //todo: report error -> not an interface
        }
        if (currentParent) finishCls();
        currentParent = parent;
        startCls();
      }
      generated += "    IFACE_METHOD(" + string(method->getName()) + ");\n";
    }
  }
  void startCls() {
    generated += "DESCRIBE_INTERFACE(" + string(currentParent->getName()) + ") {\n";
    for (const CXXBaseSpecifier base: currentParent->bases()) {
      if (auto* cls = base.getType().getTypePtrOrNull()) {
        auto* baseDecl = cls->getAsCXXRecordDecl();
        checkRecord(baseDecl, *src);
        string name;
        if (const NamespaceDecl* ns = dyn_cast<NamespaceDecl>(baseDecl->getEnclosingNamespaceContext())) {
          name += string(ns->getName()) + "::";
        }
        name += string(baseDecl->getName());
        generated += "    PARENT(" + name + ");\n";
      }
    }
  }
  void finishCls() {
    generated += "}\n";
    SourceLocation loc = currentParent->getEndLoc();
    rewriter.RemoveText(loc, 2); //KOSTYL for }; right after class locEnd
    rewriter.InsertText(loc, generated, true, false);
    generated.clear();
    generated += "};\n\n";
  }
  virtual void onEndOfTranslationUnit() override {
    if (currentParent) finishCls();
    if (!DryRun) {
      rewriter.overwriteChangedFiles();
    } else if (currentParent) {
      PresumedLoc ploc = src->getPresumedLoc(currentParent->getBeginLoc());
      outs() << "// FILE: " << ploc.getFilename() << "\n";
      rewriter.getRewriteBufferFor(ploc.getFileID())->write(outs());
      outs() << "\n";

    }
  }
  
  clang::SourceManager* src = nullptr;
  Rewriter rewriter;
  string generated = "};\n\n";
  size_t currentLine = 0;
  const clang::CXXRecordDecl* currentParent = nullptr;
};

int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser& OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  AddDescribe AddDescribe;
  MatchFinder Finder;

  Finder.addMatcher(PureMethodMatcher, &AddDescribe);
  
  return Tool.run(newFrontendActionFactory(&Finder).get());
}