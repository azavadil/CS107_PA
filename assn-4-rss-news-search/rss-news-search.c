#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "html-utils.h"
#include "vector.h" 
#include "hashset.h" 

typedef struct { 
  char *word; 
  vector articles; 
} indexEntry; 

typedef struct { 
  char *url; 
  char *title; 
  char *server;
} article; 

typedef struct { 
  vector explored; 
  hashset stopwords; 
  hashset indices; 
} rssData; 

typedef struct { 
  int articleIndex; 
  int wordcount; 
} wordcountEntry; 



static void Welcome(const char *welcomeTextFileName);
static void BuildIndices(const char *feedsFileName, rssData *allData );
static void ProcessFeed(const char *remoteDocumentName, rssData *allData );
static void PullAllNewsItems(urlconnection *urlconn, rssData *allData );
static bool GetNextItemTag(streamtokenizer *st);
static void ProcessSingleNewsItem(streamtokenizer *st, rssData *allData );
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength);
static void ParseArticle(const char *articleURL, const char* articleTitle, rssData *allData );
static void ScanArticle(streamtokenizer *st, article *a, int articleIndex, rssData *allData );
static void QueryIndices(rssData *allData);
static void ProcessResponse(const char *word, rssData *allData );
static bool WordIsWellFormed(const char *word);

static void ReadStopwords(hashset *stopwords, const char *filename); 
static void TokenizeAndBuildStopwords(hashset *stopwords, streamtokenizer *tokenMaker);
static int StringHash(const void *s, int numBuckets);
static int StringCmp(const void *elem1, const void *elem2);
static void StringFree(void *elem); 
static bool WordNotInStopwords(hashset *stopwords, char *word);
static int IndexCmp(const void* elem1, const void* elem2);
static int IndexHash(const void *vp, int numBuckets);
static void IndexFree(void *elem); 
static void PrintString(void *elemAddr, void *auxData);
static int WordcountEntryCmp(const void *vp1, const void *vp2); 
static int ReverseWordcountCmp(const void *vp1, const void *vp2); 
static void UpdateIndices(vector *resultsForWord, int articleIndex); 
static int ArticleCmp(const void *vp1, const void *vp2); 
static void PrintArticle(void *elemAddr, void *auxData); 
static void PersistArticle(article *a, const char *url, const char *title, const char *server);
static void ArticleFree(void *vp);
    
/**
 * Function: main
 * --------------
 * Serves as the entry point of the full application.
 * You'll want to update main to declare several hashsets--
 * one for stop words, another for previously seen urls, etc--
 * and pass them (by address) to BuildIndices and QueryIndices.
 * In fact, you'll need to extend many of the prototypes of the
 * supplied helpers functions to take one or more hashset *s.
 *
 * Think very carefully about how you're going to keep track of
 * all of the stop words, how you're going to keep track of
 * all the previously seen articles, and how you're going to 
 * map words to the collection of news articles where that
 * word appears
 */ 
    
static const char *const kWelcomeTextFile = "/home/compilers/media/assn-4-rss-news-search-data/welcome.txt";
static const char *const kDefaultFeedsFile = "/home/compilers/media/assn-4-rss-news-search-data/rss-feeds3.txt";


int main(int argc, char **argv)
{
  static const char *stopwordFilename = "/home/compilers/media/assn-4-rss-news-search-data/stop-words.txt"; 
  static const int kStopwordBuckets = 1009; 
  static const int kIndexNumBuckets = 10007; 

  rssData allData; 

  HashSetNew(&allData.stopwords, sizeof(char*), kStopwordBuckets, StringHash, StringCmp, StringFree); 
 
  HashSetNew(&allData.indices, sizeof(indexEntry), kIndexNumBuckets, IndexHash, IndexCmp, IndexFree); 
   
  // this vector
  VectorNew(&allData.explored, sizeof(article), ArticleFree, 10);
  
  Welcome(kWelcomeTextFile);
  ReadStopwords(&allData.stopwords, stopwordFilename); 
  BuildIndices((argc == 1) ? kDefaultFeedsFile : argv[1], &allData );

  int hcount = HashSetCount(&allData.indices); 
  printf("hcount: %d\n", hcount); 
  
  printf("Finished BuildIndices\n"); 
  QueryIndices(&allData);
  return 0;
}


/** 
 * Function: ReadStopwords
 * ------------------------
 * ReadStopwords takes an empty hashset addressed by stopwords and populates the hashset
 * with dynamically allocated C strings. 
 * 
 * The stop words themselves are stored in the file named 'stop-words.txt'. The stopwords
 * file is assumed to exist. If the stopwords file is not found the program exits. If the 
 * file exists ReadStopwords passes the buck to TokenizeAndBuildStopwords to complete the 
 * routine. 
 * 
 * @param stopwords: the empty hashset to be populated with a list of stopwords
 * 
 * @param filename : the path to the text file where stop words stored. Format is one word
 *                   line with no whitespace
 * 
 * no return value
 */ 

static void ReadStopwords(hashset *stopwords, const char *filename)
{
  FILE *infile = fopen(filename, "r"); 
  if (infile == NULL){ 
    fprintf(stderr, "Could not open Stopword file name \"%s\"\n", filename); 
    exit(1); 
  }
  
  streamtokenizer tokenMaker; 
  STNew(&tokenMaker, infile, "\n", true); 
  TokenizeAndBuildStopwords(stopwords, &tokenMaker); 
  STDispose(&tokenMaker); 
  fclose(infile); 
} 


/** 
 * Function: TokenizeAndBuildStopwords
 * ------------------------
 * TokenizeAndBuildStopwords takes an empty hashset addressed by stopwords and populates the hashset
 * with dynamically allocated C strings. 
 * 
 * @param stopwords: the empty hashset to be populated with a list of stopwords
 * 
 * @param tokenMaker : the streamtokenizer that reads in the words into a buffer one at a time. 
 * 
 * no return value
 */ 

static void TokenizeAndBuildStopwords(hashset *stopwords, streamtokenizer *tokenMaker)
{ 
  printf("loading Stopwords...\n"); 
  
  char buffer[2048]; 
  while(STNextToken(tokenMaker, buffer, sizeof(buffer))){ 
    const char *currWordPtr; 
    currWordPtr = strdup(buffer); 
    HashSetEnter(stopwords, &currWordPtr); 
  }
  printf("loaded %d words\n", HashSetCount(stopwords));
}  



/** 
 * Function: WordNotInStopwords
 * -----------------------------
 * Takes a hashset of stopwords and a word as arguments. 
 * Returns true if the word is not found in the stopwords, false otherwise. 
 * 
 * @param stopwords: hashset containing stopwords
 * @param word: search word as C string
 * 
 * return value: true if word not found, false otherwise
 */ 

static bool WordNotInStopwords(hashset *stopwords, char *word )
{ 
  char *dummy = word;
  void *found = HashSetLookup(stopwords, &dummy); 

  if (found == NULL) {
    return true; 
  } else { 
   return false;
  }
}





/** 
 * Function: Welcome
 * -----------------
 * Displays the contents of the specified file, which
 * holds the introductory remarks to be printed every time
 * the application launches.  This type of overhead may
 * seem silly, but by placing the text in an external file,
 * we can change the welcome text without forcing a recompilation and
 * build of the application.  It's as if welcomeTextFileName
 * is a configuration file that travels with the application.
 */
 
static const char *const kNewLineDelimiters = "\r\n";
static void Welcome(const char *welcomeTextFileName)
{
  FILE *infile;
  streamtokenizer st;
  char buffer[1024];
  
  infile = fopen(welcomeTextFileName, "r");
  assert(infile != NULL);    
  
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    printf("%s\n", buffer);
  }
  
  printf("\n");
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one.. 
  fclose(infile);
}

/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the hashset of indices.
 * Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remore xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name (it's
 * in the file for humans to read, but our aggregator doesn't care what the name is)
 * and then extracts the URL.  It then relies on ProcessFeed to pull the remote
 * document and index its content.
 */

static void BuildIndices(const char *feedsFileName, rssData *allData )
{
  FILE *infile;
  streamtokenizer st;
  char remoteFileName[1024];
  
  infile = fopen(feedsFileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STSkipUntil(&st, ":") != EOF) { // ignore everything up to the first selicolon of the line
    STSkipOver(&st, ": ");		 // now ignore the semicolon and any whitespace directly after it
    STNextToken(&st, remoteFileName, sizeof(remoteFileName));   
    ProcessFeed(remoteFileName, allData );
  }
  
  STDispose(&st);
  fclose(infile);
  printf("\n");
}


/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then PullAllNewsItems is tapped to actually read the feed.  Check out the
 * documentation of the PullAllNewsItems function for more information, and inspect the documentation
 * for ParseArticle for information about what the different response codes mean.
 */

static void ProcessFeed(const char *remoteDocumentName, rssData *allData )
{
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, remoteDocumentName);
  URLConnectionNew(&urlconn, &u);
  
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Ignoring...", u.serverName);
              break;
      case 200: PullAllNewsItems(&urlconn, allData );
                break;
      case 301: 
      case 302: ProcessFeed(urlconn.newUrl, allData );
                break;
      default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
		      u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage);
	       break;
  };
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/**
 * Function: PullAllNewsItems
 * --------------------------
 * Steps though the data of what is assumed to be an RSS feed identifying the names and
 * URLs of online news articles.  Check out "datafiles/sample-rss-feed.txt" for an idea of what an
 * RSS feed from the www.nytimes.com (or anything other server that syndicates is stories).
 *
 * PullAllNewsItems views a typical RSS feed as a sequence of "items", where each item is detailed
 * using a generalization of HTML called XML.  A typical XML fragment for a single news item will certainly
 * adhere to the format of the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important moment in the transformation of Benedict XVI.</description>
 *   <author>By IAN FISHER and LAURIE GOODSTEIN</author>
 *   <pubDate>Sun, 24 Apr 2005 00:00:00 EDT</pubDate>
 *   <guid isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 * PullAllNewsItems reads and discards all characters up through the opening <item> tag (discarding the <item> tag
 * as well, because once it's read and indentified, it's been pulled,) and then hands the state of the stream to
 * ProcessSingleNewsItem, which handles the job of pulling and analyzing everything up through and including the </item>
 * tag. PullAllNewsItems processes the entire RSS feed and repeatedly advancing to the next <item> tag and then allowing
 * ProcessSingleNewsItem do process everything up until </item>.
 */

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";
static void PullAllNewsItems(urlconnection *urlconn, rssData *allData ) 
{
  streamtokenizer st;
  STNew(&st, urlconn->dataStream, kTextDelimiters, false);
  while (GetNextItemTag(&st)) { // if true is returned, then assume that <item ...> has just been read and pulled from the data stream
    ProcessSingleNewsItem(&st, allData );
  }
  
  STDispose(&st);
}

/**
 * Function: GetNextItemTag
 * ------------------------
 * Works more or less like GetNextTag below, but this time
 * we're searching for an <item> tag, since that marks the
 * beginning of a block of HTML that's relevant to us.  
 * 
 * Note that each tag is compared to "<item" and not "<item>".
 * That's because the item tag, though unlikely, could include
 * attributes and perhaps look like any one of these:
 *
 *   <item>
 *   <item rdf:about="Latin America reacts to the Vatican">
 *   <item requiresPassword=true>
 *
 * We're just trying to be as general as possible without
 * going overboard.  (Note that we use strncasecmp so that
 * string comparisons are case-insensitive.  That's the case
 * throughout the entire code base.)
 */

static const char *const kItemTagPrefix = "<item";
static bool GetNextItemTag(streamtokenizer *st)
{
  char htmlTag[1024];
  while (GetNextTag(st, htmlTag, sizeof(htmlTag))) {
    if (strncasecmp(htmlTag, kItemTagPrefix, strlen(kItemTagPrefix)) == 0) {
      return true;
    }
  }	 
  return false;
}

/**
 * Function: ProcessSingleNewsItem
 * -------------------------------
 * Code which parses the contents of a single <item> node within an RSS/XML feed.
 * At the moment this function is called, we're to assume that the <item> tag was just
 * read and that the streamtokenizer is currently pointing to everything else, as with:
 *   
 *      <title>Carrie Underwood takes American Idol Crown</title>
 *      <description>Oklahoma farm girl beats out Alabama rocker Bo Bice and 100,000 other contestants to win competition.</description>
 *      <link>http://www.nytimes.com/frontpagenews/2841028302.html</link>
 *   </item>
 *
 * ProcessSingleNewsItem parses everything up through and including the </item>, storing the title, link, and article
 * description in local buffers long enough so that the online new article identified by the link can itself be parsed
 * and indexed.  We don't rely on <title>, <link>, and <description> coming in any particular order.  We do asssume that
 * the link field exists (although we can certainly proceed if the title and article descrption are missing.)  There
 * are often other tags inside an item, but we ignore them.
 */

static const char *const kItemEndTag = "</item>";
static const char *const kTitleTagPrefix = "<title";
static const char *const kDescriptionTagPrefix = "<description";
static const char *const kLinkTagPrefix = "<link";
static void ProcessSingleNewsItem(streamtokenizer *st, rssData *allData)
{
  char htmlTag[1024];
  char articleTitle[1024];
  char articleDescription[1024];
  char articleURL[1024];
  articleTitle[0] = articleDescription[0] = articleURL[0] = '\0';
  
  int count = 0; 
 
  while (GetNextTag(st, htmlTag, sizeof(htmlTag)) && (strcasecmp(htmlTag, kItemEndTag) != 0)) {
    if (strncasecmp(htmlTag, kTitleTagPrefix, strlen(kTitleTagPrefix)) == 0) ExtractElement(st, htmlTag, articleTitle, sizeof(articleTitle));
    if (strncasecmp(htmlTag, kDescriptionTagPrefix, strlen(kDescriptionTagPrefix)) == 0) ExtractElement(st, htmlTag, articleDescription, sizeof(articleDescription));
    if (strncasecmp(htmlTag, kLinkTagPrefix, strlen(kLinkTagPrefix)) == 0) ExtractElement(st, htmlTag, articleURL, sizeof(articleURL));
    count++; 
    if (count == 5 ) break;
  }
  
  if (strncmp(articleURL, "", sizeof(articleURL)) == 0) return;     // punt, since it's not going to take us anywhere
  
  ParseArticle(articleURL, articleTitle, allData); 

}

/**
 * Function: ExtractElement
 * ------------------------
 * Potentially pulls text from the stream up through and including the matching end tag.  It assumes that
 * the most recently extracted HTML tag resides in the buffer addressed by htmlTag.  The implementation
 * populates the specified data buffer with all of the text up to but not including the opening '<' of the
 * closing tag, and then skips over all of the closing tag as irrelevant.  Assuming for illustration purposes
 * that htmlTag addresses a buffer containing "<description" followed by other text, these three scanarios are
 * handled:
 *
 *    Normal Situation:     <description>http://some.server.com/someRelativePath.html</description>
 *    Uncommon Situation:   <description></description>
 *    Uncommon Situation:   <description/>
 *
 * In each of the second and third scenarios, the document has omitted the data.  This is not uncommon
 * for the description data to be missing, so we need to cover all three scenarious (I've actually seen all three.)
 * It would be quite unusual for the title and/or link fields to be empty, but this handles those possibilities too.
 */
 
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength)
{
  assert(htmlTag[strlen(htmlTag) - 1] == '>');
  if (htmlTag[strlen(htmlTag) - 2] == '/') return;    // e.g. <description/> would state that a description is not being supplied
  STNextTokenUsingDifferentDelimiters(st, dataBuffer, bufferLength, "<");
  RemoveEscapeCharacters(dataBuffer);
  if (dataBuffer[0] == '<') strcpy(dataBuffer, "");  // e.g. <description></description> also means there's no description
  STSkipUntil(st, ">");
  STSkipOver(st, ">");
}

/** 
 * Function: ParseArticle
 * ----------------------
 * Attempts to establish a network connect to the news article identified by the three
 * parameters.  The network connection is either established of not.  The implementation
 * is prepared to handle a subset of possible (but by far the most common) scenarios,
 * and those scenarios are categorized by response code:
 *
 *    0 means that the server in the URL doesn't even exist or couldn't be contacted.
 *    200 means that the document exists and that a connection to that very document has
 *        been established.
 *    301 means that the document has moved to a new location
 *    302 also means that the document has moved to a new location
 *    4xx and 5xx (which are covered by the default case) means that either
 *        we didn't have access to the document (403), the document didn't exist (404),
 *        or that the server failed in some undocumented way (5xx).
 *
 * The are other response codes, but for the time being we're punting on them, since
 * no others appears all that often, and it'd be tedious to be fully exhaustive in our
 * enumeration of all possibilities.
 */

static void ParseArticle(const char *articleURL, const char *articleTitle, rssData *allData)
{
  url u;
  urlconnection urlconn;
  streamtokenizer st;
  int articleIndex; 

  URLNewAbsolute(&u, articleURL);
  
  
  /* check to see if we've previously scanned the article. If the article we're processing 
   * has already been scanned release the url and return 
   */ 

  article a = {articleURL, articleTitle, u.serverName}; 


  if(VectorSearch(&allData->explored, &a, ArticleCmp, 0, false) >= 0) { 
    printf("[Pass. article already indexed: \"%s\"]\n", articleTitle); 
    URLDispose(&u); 
    return; 
  }

  URLConnectionNew(&urlconn, &u);
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
	      break;
      case 200: printf("Scanning \"%s\" from \"http://%s\"\n", articleTitle, u.serverName);
	        STNew(&st, urlconn.dataStream, kTextDelimiters, false);
		PersistArticle(&a, articleURL, articleTitle, u.serverName); 
		VectorAppend(&allData->explored, &a);
		articleIndex = VectorLength(&allData->explored)-1; 
		ScanArticle(&st, &a, articleIndex, allData);
		STDispose(&st);
		break;
      case 301:
      case 302: // just pretend we have the redirected URL all along, though index using the new URL and not the old one...
	        ParseArticle(urlconn.newUrl, articleTitle, allData );
		break;
      default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.serverName, urlconn.responseCode);
	       break;
  }
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/**
 * Function: ScanArticle
 * ---------------------
 * Parses the specified article, skipping over all HTML tags, and counts the numbers
 * of well-formed words that could potentially serve as keys in the set of indices.
 * Once the full article has been scanned, the number of well-formed words is
 * printed, and the longest well-formed word we encountered along the way
 * is printed as well.
 *
 * This is really a placeholder implementation for what will ultimately be
 * code that indexes the specified content.
 * 
 * Each indexEntry is a word/vector<wordcountEntry> pair (i.e. each word carries with it 
 * a vector of the articles the word has appeared in).
 * 
 * Each wordcountEntry is a pair of an articleIndex and a wordcount. The articleIndex is 
 * unique. We take advantage of that and use the articleIndex as a key/unique identifier
 *  
 * We scan in each word. If the word is not in the index, then we add an indexEntry to the 
 * index (with an empty article vector). 
 * 
 * We then call UpdateIndices to fill out the vector<wordcountIndex> portion of the indexEntry
 * pair. 
 *
 */


static void ScanArticle(streamtokenizer *st, article *a, int articleIndex, rssData *allData )
{
  int numWords = 0;
  char word[1024];
  char longestWord[1024] = {'\0'};

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWellFormed(word)) {
	numWords++;	
	char *dummy = word;
	
	
	
	if ( WordNotInStopwords(&allData->stopwords, word)) { 
	  /**  Try looking up the word. If the word is not in the indices, create a new indexEntry 
	   *   initialized with the word and an empty vector and enter it into the hashset
	   */
	  indexEntry entry = {word}; 
	  indexEntry *found = HashSetLookup(&allData->indices, &entry);

	  if (found == NULL) {  
	    entry.word = strdup(dummy); 
	    VectorNew(&entry.articles, sizeof(wordcountEntry), NULL, 10); 
	    HashSetEnter(&allData->indices, &entry);
	  }

	  // now we act as if the entry was in the index all along
	  found  = (indexEntry*)HashSetLookup( &allData->indices, &entry); 	  
	  


	  UpdateIndices(&found->articles, articleIndex);

	}
	if (strlen(word) > strlen(longestWord))
	  strcpy(longestWord, word);
      }
    }
  }

  printf("\tWe counted %d well-formed words [including duplicates].\n", numWords);
  printf("\tThe longest word scanned was \"%s\".", longestWord);
  if (strlen(longestWord) >= 15 && (strchr(longestWord, '-') == NULL)) 
    printf(" [Ooooo... long word!]");
  printf("\n");
}

/** 
 * Function: UpdateIndices
 * ----------------------
 * Updates the indices for the current result. If the url is already in the 
 * list of results, then we increment the wordcount. Otherwise we add the article
 * to the list of results. 
 */

static void UpdateIndices(vector *articlesForWord, int articleIndex){ 

  // initialize a wordcountEntry with the articleIndex and a wordcount of 0
  wordcountEntry newWordcount; 
  newWordcount.articleIndex = articleIndex; 
  newWordcount.wordcount =  0;  

  int idx = VectorSearch( articlesForWord, &newWordcount, WordcountEntryCmp, 0, false); 
  
  // if the wordcountEntry isn't in the vector, add the entry
  if (idx == -1) { 
    VectorAppend( articlesForWord, &newWordcount); 
  } else {
    wordcountEntry *found = (wordcountEntry*)VectorNth(articlesForWord, idx);
    found->wordcount++; 
  } 
} 
    


/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 */

static void QueryIndices(rssData *allData)
{
  char response[1024];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(response, allData);
  }

  // free the memory when we're finished 
  HashSetDispose(&allData->indices); 
  HashSetDispose(&allData->stopwords); 
  VectorDispose(&allData->explored); 

}

/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 */

static void ProcessResponse(const char *word, rssData *allData)
{

  if (WordIsWellFormed(word)) {
        void *found = HashSetLookup(&allData->indices, &word); 
	if (found != NULL) {
	  indexEntry *entry = (indexEntry*)found;
	  VectorSort(&entry->articles, ReverseWordcountCmp); 
	  VectorMap(&entry->articles, PrintArticle, &allData->explored);
	  	  
	} else {
	  printf("\tWord not found in our indices\n");
	} 
  } else {
	printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", word);
  }
  
}  

/**
 * Predicate Function: WordIsWellFormed
 * ------------------------------------
 * Before we allow a word to be inserted into our map
 * of indices, we'd like to confirm that it's a good search term.
 * One could generalize this function to allow different criteria, but
 * this version hard codes the requirement that a word begin with 
 * a letter of the alphabet and that all letters are either letters, numbers,
 * or the '-' character.  
 */

static bool WordIsWellFormed(const char *word)
{
  int i;
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}





/** 
 * Function: StringCmp
 * --------------------
 * StringCmp takes two const void * as arguments and casts them to be the const char**
 * we know them to be. We then dereference the const char ** to get two C strings. 
 * 
 * We call strcasecmp with the two C strings to perform a case insensitive comparison. 
 * 
 * @param elem1: the address of the first string 
 * @param elem2: the address of the second string
 * @return:      a positive, negative, or zero depending on whether the first string is greater
 *               than, less than, or equal to the second string
 */ 

static int StringCmp(const void* elem1, const void* elem2){ 
  return strcasecmp(*(const char**)elem1, *(const char**)elem2); 
} 



/** 
 * Function: StringFree
 * ---------------------
 * StringFree frees the memory allocated to the char* address provided by 
 * the void*. The char* points to dynamically allocated char arrays generated
 * by strdup. We use the stdlib free function to return memory to the heap. 
 * 
 * @param elem: address of dynamically allocated string to be freed
 * 
 * no return value
 */  

static void StringFree(void *elem){ 
  free(*(char**)elem); 
} 



/** 
 * StringHash                     
 * ----------  
 * This function is adapted from Eric Roberts' "The Art and Science of C"
 * It takes a string and uses it to derive a hash code, which   
 * is an integer in the range [0, numBuckets).  The hash code is computed  
 * using a method called "linear congruence."  A similar function using this     
 * method is described on page 144 of Kernighan and Ritchie.  The choice of                                                     
 * the value for the kHashMultiplier can have a significant effect on the                            
 * performance of the algorithm, but not on its correctness.                                                    
 * This hash function has the additional feature of being case-insensitive,  
 * hashing "Peter Pawlowski" and "PETER PAWLOWSKI" to the same code.
 *
 * Note that elem is a const void *, as it needs to be if the StringHash
 * routine is to be used as a HashSetHashFunction.  In this case, then
 * const void * is really a const char ** in disguise.
 *
 * @param elem the address of the C string to be hashed.
 * @param numBuckets the number of buckets in the hashset doing the hashing.
 * @return the hash code of the string addressed by elem.
 */  

static const signed long kHashMultiplier = -1664117991L;

static int StringHash(const void *vp, int numBuckets)  
{            
  unsigned long hashcode = 0;
  char *s = *(const char**)vp; 
  
  for (int i = 0; i < strlen(s); i++)  
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);  
  
  return hashcode % numBuckets;                                
}



/** 
 * Function: IndexCmp
 * ------------------- 
 * IndexCmp takes two const void * as arguments and 
 * casts the arguments to the indexEntry * we know them to be. 
 * We then access the word field of the indexEntry (However, we could 
 * use our knowledge that the word field (type char *) sits at the 
 * base address of the indexEntry struct and apply strcasecmp directly 
 * to the indexEntry * address).
 * 
 * Note: we might consider calling StringCmp instead of using 
 *       strcasecmp directly so that all string comparisons are centralized
 *       in the single StringCmp function 
 * 
 * @param elem1: the address of the first indexEntry 
 * @param elem2: the address of the second indexEntry
 * @return:      a positive, negative, or zero depending on whether the first string is greater
 *               than, less than, or equal to the second string
 */ 

static int IndexCmp(const void* elem1, const void* elem2)
{
  indexEntry *e1 = (indexEntry*)elem1; 
  indexEntry *e2 = (indexEntry*)elem2; 
  return strcasecmp(e1->word, e2->word); 
}
 


/** 
 * Function: IndexHash
 * -------------------
 * Hashes an index by applying the string hash algorithm to the 
 * word field of an indexEntry. 
 * 
 * Take a const void * and int as arguments. We cast the void * 
 * to the indexEntry * we know it to be and then apply the StringHash 
 * to the word field
 * 
 * Note: we could also we use our knowledge of the indexEntry struct 
 *       to write more succintly. We know that the word key sits at 
 *       the base address of the struct. Therefore, we could cast the 
 *        base address of the indexEntry to a (char**) and dereference 
 *        to get a C string 
 *
 * @param vp: the indexEntry * 
 * @param numBuckets: the number of hash buckets to use
 * 
 * 
 */ 

static int IndexHash(const void *vp, int numBuckets)
{ 
  const indexEntry *entry = vp;   
  return StringHash(&entry->word, numBuckets); 
}



/** 
 * Function: IndexFree
 * --------------------
 * IndexFree frees memory allocated to the index entry
 * 
 * @param elem: address of the indexEntry to be reclaimed
 * 
 * no return value
 */ 

static void IndexFree(void *elem)
{ 
  indexEntry *entry = elem; 
  free( entry->word ); 
  VectorDispose(&entry->articles); 
}



/** 
 * Function: WordcountEntryCmp
 * -----------------------
 * takes two void* as arguments. We immediately cast the void* to the wordcountEntry 
 * we know it to be. We then compare the articleIndex field of the wordcountEntry. 
 * 
 * @param vp1:  the address of the first wordcountEntry to be compared 
 * @param vp2:  the address of the second wordcount entry to be compared 
 * 
 * return value: negative, positive, or 0 if the first wordcountEntry->articleIndex 
 *               it less than, greater than, or equal to the second wordcountEntry
 */ 

static int WordcountEntryCmp(const void *vp1, const void *vp2)
{
  const wordcountEntry *wc1 = vp1; 
  const wordcountEntry *wc2 = vp2; 
 
  return wc1->articleIndex - wc2->articleIndex; 
} 



/** 
 * Function: ReverseWordcountCmp
 * -----------------------
 * takes two void* as arguments. We immediately cast the void* to the wordcountEntry 
 * we know it to be. We then compare the wordcount field of the wordcountEntry. 
 * 
 * ReverseWordcountCmp is used to sort articles by the frequency of word appearence. 
 * We want the article where the word appeared most frequently to appear first so 
 * we use a reverse comparison function. 
 * 
 * @param vp1:  the address of the first wordcountEntry to be compared 
 * @param vp2:  the address of the second wordcountEntry to be compared 
 * 
 * return value: negative, positive, or 0 if the first wordcountEntry->wordcount 
 *               it less than, greater than, or equal to the second wordcountEntry
 */ 


static int ReverseWordcountCmp(const void *vp1, const void *vp2)
{
  const wordcountEntry *wc1 = vp1; 
  const wordcountEntry *wc2 = vp2; 
 
  return wc2->wordcount - wc1->wordcount; 
}


/** 
 * Function: PersistArticle
 * -------------------------
 * PersistArticle dynamically allocates C strings to the specified article. 
 * 
 * @ param a: address of the article to be persisted
 * @ param url: address of the string to be copied into the articles url field
 * @ param title: address of the string to be copied into the articles title field
 * @ param server: address of the string to be copied into the articles server field
 * 
 * no return value
 */ 

static void PersistArticle(article *a, const char *url, const char *title, const char *server)
{ 
  a->url = strdup(url); 
  a->title = strdup(title); 
  a->server = strdup(server); 
} 

/** 
 * Function: ArticleCmp
 * ---------------------
 * ArticleCmp takes two void* as arguments and casts them to the article* 
 * we know them to be. If the articles have the same title and came from the 
 * the same server, then we consider them equal. 
 * 
 * If the articles have the same URL we consider them equal. 
 * 
 * @param vp1:  the address of the first article
 * @param vp2:  the address of the second article 
 * 
 * return type: negative, positive, 0 if the first article is less than,  
 *               greater than, equal to the second article.
 */ 

static int ArticleCmp(const void *vp1, const void *vp2){ 
  
  const article *a1 = vp1; 
  const article *a2 = vp2; 

  if(StringCmp(&a1->title,&a2->title) == 0 && StringCmp(&a1->server,&a2->server) == 0) return 0;  

  return StringCmp(&a1->url, &a2->url);
}




/** 
 * Function ArticleFree
 * ---------------------
 * Frees the 3 dynamically allocated strings that comprise an article struct
 *
 * @param vp: address of the article to be releases
 * 
 * no return value
 */ 

static void ArticleFree(void *vp)
{
  article *a = vp; 
  StringFree(&a->url); 
  StringFree(&a->title); 
  StringFree(&a->server); 
}

/** 
 * Fuction: PrintString
 * ---------------------
 * PrintString is not used. Was used in early debugging 
 */ 

static void PrintString(void *elemAddr, void *auxData)
{ 
   char *word = *(char**)elemAddr; 
   printf("%s\n", word); 
 } 



/** 
 * Function: PrintArticle
 * -------------------
 * PrintArticle formats and prints an article struct. PrintArticle is used in QueryIndices. 
 * When we query the indices for a word, if the word is found then we use PrintArticle
 * to format and print the articles related to the URL 
 */ 

static void PrintArticle(void *elemAddr, void *auxData)
{    

  wordcountEntry *currEntry = elemAddr; 
  article *a = VectorNth(auxData, currEntry->articleIndex); 

   char *url = a->url;
   char *title = a->title; 
   int wordcount = currEntry->wordcount; 

   printf("\tTitle: %s\t[Number of times seen: %d]\n\tLink: %s\n", title, wordcount, url); 
}




