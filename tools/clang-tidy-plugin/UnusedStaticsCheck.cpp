#include "UnusedStaticsCheck.h"
#include "Utils.h"
// Explicit, for LLVM_VERSION_MAJOR used by the Linkage guard below. It arrives
// transitively through most clang headers, but depending on that is fragile:
// if the macro were ever undefined the #if silently evaluates 0 >= 18 as false
// and selects the pre-18 branch.
#include <llvm/Config/llvm-config.h>
#include <unordered_map>

using namespace clang::ast_matchers;

namespace clang::tidy::cata
{

void UnusedStaticsCheck::registerMatchers( MatchFinder *Finder )
{
    Finder->addMatcher(
        varDecl(
            anyOf( hasParent( namespaceDecl() ), hasParent( translationUnitDecl() ) ),
            hasStaticStorageDuration()
        ).bind( "decl" ),
        this
    );
    Finder->addMatcher(
        declRefExpr( to( varDecl().bind( "decl" ) ) ).bind( "ref" ),
        this
    );
}

void UnusedStaticsCheck::check( const MatchFinder::MatchResult &Result )
{
    const VarDecl *ThisDecl = Result.Nodes.getNodeAs<VarDecl>( "decl" );
    if( !ThisDecl ) {
        return;
    }

    const DeclRefExpr *Ref = Result.Nodes.getNodeAs<DeclRefExpr>( "ref" );
    if( Ref ) {
        used_decls_.insert( ThisDecl );
    }

    const SourceManager &SM = *Result.SourceManager;

    // Ignore cases in header files
    if( !SM.isInMainFile( ThisDecl->getBeginLoc() ) ) {
        return;
    }

    // Ignore cases that are not the first declaration
    if( ThisDecl->getPreviousDecl() ) {
        return;
    }

    // Ignore cases that are not static linkage
    Linkage Lnk = ThisDecl->getFormalLinkage();
    // THE ONLY CONSTRUCT HERE THAT NEEDS A VERSION GUARD. Linkage became a
    // SCOPED enum in LLVM 18: the enumerators moved from InternalLinkage /
    // UniqueExternalLinkage to Linkage::Internal / Linkage::UniqueExternal, and
    // measured, NO single spelling compiles under both 17 and 21. The other
    // three LLVM 18+ fixes in this plugin (ends_with/starts_with, operator==,
    // FileEntryRef) all have forms accepted by both, so they carry no guard.
    // Upstream needs no guard at all because it targets one modern LLVM; this
    // fork supports 17 and 21 together, which is why the conditional exists.
#if LLVM_VERSION_MAJOR >= 18
    if( Lnk != Linkage::Internal && Lnk != Linkage::UniqueExternal ) {
#else
    if( Lnk != InternalLinkage && Lnk != UniqueExternalLinkage ) {
#endif
        return;
    }

    SourceLocation DeclLoc = ThisDecl->getBeginLoc();
    SourceLocation ExpansionLoc = SM.getFileLoc( DeclLoc );
    if( DeclLoc != ExpansionLoc ) {
        // We are inside a macro expansion
        return;
    }

    decls_.push_back( ThisDecl );
}

void UnusedStaticsCheck::onEndOfTranslationUnit()
{
    for( const VarDecl *V : decls_ ) {
        if( used_decls_.count( V ) ) {
            continue;
        }

        diag( V->getBeginLoc(), "Variable %0 declared but not used." ) << V;
    }
}

} // namespace clang::tidy::cata
