/* hubClone - Clone the hub text files to a local copy, fixing up bigDataUrls
 * to remote location if necessary. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "trackDb.h"
#include "cart.h" // can't include trackHub.h without this?
#include "trackHub.h"
#include "errCatch.h"
#include "ra.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubClone - Clone the remote hub text files to a local copy in newDirectoryName, fixing up bigDataUrls to remote location if necessary\n"
  "usage:\n"
  "   hubClone http://url/to/hub.txt\n"
  "options:\n"
  "   -udcDir=/dir/to/udcCache   Path to udc directory\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void polishHubName(char *name)
/* Helper function for making somewhat safe directory names. Changes non-alpha to '_' */
{
if (name == NULL)
    return;

char *in = name;
char c;

for(; (c = *in) != 0; in++)
    {
    if (!(isalnum(c) || c == '-' || c == '_'))
        *in = '_';
    }
}

void printHubStanza(struct hash *stanza, FILE *out, char *baseUrl)
/* print hub.txt stanza to out */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
fprintf(out, "%s %s\n", "hub", (char *)hashFindVal(stanza, "hub"));
for (hel = helList; hel != NULL; hel = hel->next)
    {
    if (!sameString(hel->name, "hub"))
        {
        if (sameString(hel->name, "descriptionUrl"))
            fprintf(out, "%s %s\n", hel->name, trackHubRelativeUrl(baseUrl, hel->val));
        else
            fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
        }
    }
fprintf(out, "\n");
hashElFreeList(&helList);
}

void printGenomeStanza(struct hash *stanza, FILE *out, char *baseUrl, boolean oneFile)
/* print genomes.txt stanza to out */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
char *genome = (char *)hashFindVal(stanza, "genome");

fprintf(out, "%s %s\n", "genome", genome);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    if (!sameString(hel->name, "genome"))
        {
        if (sameString(hel->name, "groups") ||
            sameString(hel->name, "twoBitPath") ||
            sameString(hel->name, "htmlPath")
            )
            fprintf(out, "%s %s\n", hel->name, trackHubRelativeUrl(baseUrl, hel->val));
        else if (sameString(hel->name, "trackDb"))
            {
            if (oneFile)
                {
                fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
                }
            else
                {
                // some assembly hubs use different directory names than the typical
                // genomeName/trackDb.txt setup, hardcode this so assembly hub will
                // still load locally
                char *tdbFileName = NULL;
                if ((tdbFileName = strrchr((char *)hel->val, '/')) != NULL)
                    tdbFileName += 1;
                else
                    tdbFileName = (char *)hel->val;
                fprintf(out, "%s %s/%s\n", hel->name, genome, tdbFileName);
                }
            }
        else
            fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
        }
    }
fprintf(out, "\n");
hashElFreeList(&helList);
}

void printTrackDbStanza(struct hash *stanza, FILE *out, char *baseUrl)
/* print a trackDb stanza but with relative references replaced by remote links */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
fprintf(out, "%s %s\n", "track", (char *)hashFindVal(stanza, "track"));
for (hel = helList; hel != NULL; hel = hel->next)
    {
    if (!sameString(hel->name, "track"))
        {
        if (sameString(hel->name, "bigDataUrl") ||
            sameString(hel->name, "bigDataIndex") ||
            sameString(hel->name, "barChartMatrixUrl") ||
            sameString(hel->name, "barChartSampleUrl") ||
            sameString(hel->name, "linkDataUrl") ||
            sameString(hel->name, "frames") ||
            sameString(hel->name, "summary") ||
            sameString(hel->name, "searchTrix") ||
            sameString(hel->name, "html")
            )
            fprintf(out, "%s %s\n", hel->name, trackHubRelativeUrl(baseUrl, hel->val));
        else
            fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
        }
    }
fprintf(out, "\n");
hashElFreeList(&helList);
}

void printGenericStanza(struct hash *stanza, FILE *out, char *baseUrl)
/* print a hash to out */
{
struct hashEl *hel, *helList = hashElListHash(stanza);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    fprintf(out, "%s %s\n", hel->name, (char *)hel->val);
    }
fprintf(out,"\n");
}

void printOneFile(char *url, FILE *f, boolean oneFile)
/* printOneFile: pass a stanza to appropriate printer */
{
struct lineFile *lf;
struct hash *stanza;
struct hashEl *includeFile;

lf = udcWrapShortLineFile(url, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
while ((stanza = raNextRecord(lf)) != NULL)
    {
    if (hashLookup(stanza, "hub"))
        {
        printHubStanza(stanza, f, url);
        }
    else if (hashLookup(stanza, "genome"))
        {
        printGenomeStanza(stanza, f, url, oneFile);
        }
    else if (hashLookup(stanza, "track"))
        {
        printTrackDbStanza(stanza, f, url);
        }
    else
        {
        // if there's an include file then open and print the include file
        includeFile = hashLookup(stanza, "include");
        if (includeFile != NULL)
            {
            char *newUrl = trackHubRelativeUrl(url, includeFile->val);
            printOneFile(newUrl, f, oneFile);
            }
        else
            printGenericStanza(stanza, f, url);
        }
    }
lineFileClose(&lf);
freeHash(&stanza);
}

struct trackHub *readHubFromUrl(char *hubUrl)
/* readHubUrl: errCatch around trackHubOpen */
{
struct trackHub *hub = NULL;
struct errCatch *errCatch = errCatchNew();

if (errCatchStart(errCatch))
    hub = trackHubOpen(hubUrl, "");
errCatchEnd(errCatch);
if (errCatch->gotError)
    errAbort("aborting: %s\n", errCatch->message->string);
return hub;
}

FILE *createPathAndFile(char *path)
/* if path contains parent directories that don't exist, create them first before opening file */
{
char *copy = cloneString(path);
if (stringIn("/", copy))
    {
    chopSuffixAt(copy, '/');
    makeDirs(copy);
    // now make the real file
    return mustOpen(path, "w");
    }
return mustOpen(path, "w");
}

void createWriteAndCloseFile(char *fileName, char *url, boolean useOneFile)
/* Wrapper around a couple lines */
{
FILE *f;
f = createPathAndFile(fileName);
printOneFile(url, f, useOneFile);
carefulClose(&f);
}

void hubClone(char *hubUrl)
/* hubClone - Clone the hub text files to a local copy, fixing up bigDataUrls 
 * to remote locations if necessary. */
{
struct trackHub *hub;
struct trackHubGenome *genome;
char *hubBasePath, *hubName, *hubFileName;
char *genomesUrl, *genomesDir, *genomesFileName;
char *tdbFileName, *tdbFilePath;
char *path; 
FILE *f;
boolean oneFile = FALSE;

hubBasePath = cloneString(hubUrl);
chopSuffixAt(hubBasePath, '/'); // don't forget to add a "/" back on!
hubFileName = strrchr(hubUrl, '/'); 
hubFileName += 1;

hub = readHubFromUrl(hubUrl);
if (hub == NULL)
    errAbort("error opening %s", hubUrl);

hubName = cloneString((char *)hashFindVal(hub->settings, "hub"));
polishHubName(hubName);

if (trackHubSetting(hub, "useOneFile"))
    {
    oneFile = TRUE;
    makeDirs(hubName);
    path = catTwoStrings(hubName, catTwoStrings("/", hubFileName));
    f = mustOpen(path, "w");
    printOneFile(hubUrl, f, oneFile);
    carefulClose(&f);
    }
else
    {
    genome = hub->genomeList;
    if (genome == NULL)
        errAbort("error opening %s file", hub->genomesFile);

    path = catTwoStrings(hubName, catTwoStrings("/", hubFileName));
    createWriteAndCloseFile(path, hubUrl, oneFile);

    genomesUrl = trackHubRelativeUrl(hub->url, hub->genomesFile);
    genomesFileName = catTwoStrings(hubName, catTwoStrings("/", hub->genomesFile));
    char *genomePath = cloneString(genomesFileName);
    chopSuffixAt(genomePath, '/'); // used later for making the right directory structure

    createWriteAndCloseFile(genomesFileName, genomesUrl, oneFile);

    for (; genome != NULL; genome = genome->next)
        {
        if (startsWith("_", genome->name)) // assembly hubs have a leading '_'
            genome->name += 1;

        // make correct directory strucutre
        genomesDir = catTwoStrings(genomePath, catTwoStrings("/", genome->name));
        tdbFileName = strrchr(genome->trackDbFile, '/') + 1;
        tdbFilePath = catTwoStrings(genomesDir, catTwoStrings("/", tdbFileName));
        createWriteAndCloseFile(tdbFilePath, genome->trackDbFile, oneFile);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
hubClone(argv[1]);
return 0;
}
