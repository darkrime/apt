// Includes								/*{{{*/
#include <apt-pkg/error.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/init.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/version.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/metaindex.h>

#include <apti18n.h>

#include "private-output.h"
#include "private-cacheset.h"
									/*}}}*/

namespace APT {
   namespace Cmd {

// DisplayRecord - Displays the complete record for the package		/*{{{*/
// ---------------------------------------------------------------------
bool DisplayRecord(pkgCacheFile &CacheFile, pkgCache::VerIterator V,
                   ostream &out)
{
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return false;

   // Find an appropriate file
   pkgCache::VerFileIterator Vf = V.FileList();
   for (; Vf.end() == false; ++Vf)
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) == 0)
	 break;
   if (Vf.end() == true)
      Vf = V.FileList();
      
   // Check and load the package list file
   pkgCache::PkgFileIterator I = Vf.File();
   if (I.IsOk() == false)
      return _error->Error(_("Package file %s is out of sync."),I.FileName());

   // Read the record
   FileFd PkgF;
   if (PkgF.Open(I.FileName(), FileFd::ReadOnly, FileFd::Extension) == false)
      return false;
   pkgTagSection Tags;
   pkgTagFile TagF(&PkgF);

   if (TagF.Jump(Tags, V.FileList()->Offset) == false)
      return _error->Error("Internal Error, Unable to parse a package record");

   // make size nice
   std::string installed_size;
   if (Tags.FindI("Installed-Size") > 0)
      installed_size = SizeToStr(Tags.FindI("Installed-Size")*1024);
   else
      installed_size = _("unknown");
   std::string package_size;
   if (Tags.FindI("Size") > 0)
      package_size = SizeToStr(Tags.FindI("Size"));
   else
      package_size = _("unknown");

   TFRewriteData RW[] = {
      {"Conffiles",0},
      {"Description",0},
      {"Description-md5",0},
      {"Installed-Size", installed_size.c_str(), 0},
      {"Size", package_size.c_str(), "Download-Size"},
      {}
   };
   if(TFRewrite(stdout, Tags, NULL, RW) == false)
      return _error->Error("Internal Error, Unable to parse a package record");

   // write the description
   pkgRecords Recs(*Cache);
   // FIXME: show (optionally) all available translations(?)
   pkgCache::DescIterator Desc = V.TranslatedDescription();
   if (Desc.end() == false)
   {
      pkgRecords::Parser &P = Recs.Lookup(Desc.FileList());
      out << "Description: " << P.LongDesc();
   }
   
   // write a final newline (after the description)
   out << std::endl << std::endl;

   return true;
}
									/*}}}*/
bool ShowPackage(CommandLine &CmdL)					/*{{{*/
{
   pkgCacheFile CacheFile;
   CacheSetHelperVirtuals helper(true, GlobalError::NOTICE);
   APT::VersionList::Version const select = _config->FindB("APT::Cache::AllVersions", false) ?
			APT::VersionList::ALL : APT::VersionList::CANDIDATE;
   APT::VersionList const verset = APT::VersionList::FromCommandLine(CacheFile, CmdL.FileList + 1, select, helper);
   for (APT::VersionList::const_iterator Ver = verset.begin(); Ver != verset.end(); ++Ver)
      if (DisplayRecord(CacheFile, Ver, c1out) == false)
	 return false;

   if (select == APT::VersionList::CANDIDATE)
   {
      APT::VersionList const verset_all = APT::VersionList::FromCommandLine(CacheFile, CmdL.FileList + 1, APT::VersionList::ALL, helper);
      if (verset_all.size() > verset.size())
         _error->Notice(ngettext("There is %lu additional record. Please use the '-a' switch to see it", "There are %lu additional records. Please use the '-a' switch to see them.", verset_all.size() - verset.size()), verset_all.size() - verset.size());
   }

   for (APT::PackageSet::const_iterator Pkg = helper.virtualPkgs.begin();
	Pkg != helper.virtualPkgs.end(); ++Pkg)
   {
       c1out << "Package: " << Pkg.FullName(true) << std::endl;
       c1out << "State: " << _("not a real package (virtual)") << std::endl;
       // FIXME: show providers, see private-cacheset.h
       //        CacheSetHelperAPTGet::showVirtualPackageErrors()
   }

   if (verset.empty() == true)
   {
      if (helper.virtualPkgs.empty() == true)
        return _error->Error(_("No packages found"));
      else
        _error->Notice(_("No packages found"));
   }

   return true;
}
									/*}}}*/
} // namespace Cmd
} // namespace APT
