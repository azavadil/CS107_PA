using namespace std;  
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"

/** Implementation notes: data format
 * ---------------------------------
 * The program relies on data files that are configured to be both compact and allow
 * fast access. 
 * 
 * The two data files are the actorFile and the movieFile. The general structure is 
 * number of actors/movies | offsets | records
 * 
 * actorFile:   the actorFile is configured as follows
 *              first 4 bytes store the number of actors as an int
 *            	
 * 				next 4 bytes (i.e. bytes 5 - 4*number of actors) stores the offsets
 * 				to the 0th actor, 1th actor, ...  
 *            	
 * 				next chunk stores the actor records themselves
 * 
 * actorRecord: actor name laid out character by character. If the length of the actor 
 *              name plus the terminating '\0' is odd, then padding of '\0' to ensure 
 *              the length is even
 *              
 * 				number of movies the actor has appeared in expressed as a two-byte short
 *              
 *				An array of offsets into the movieFile where each offset identifies a movie
 * 
 * movieFile:   the movieFile is configured as follows,  number of movies | offsets | movie records
 *              first 4 bytes store the number of movies as an int
 *              
 *              next 4 bytes (i.e. bytes 5 - 5*number of movies) stores the offsets to the 0th movie, 
 *              1th movie, ... 
 * 
 *  		    next chunk are the movie records
 * 
 * movieRecord  Title of the movie terminated by a '\0'
 *           
 *              Year the film was released expressed as a single byte. If the total number of bytes used 
 * 			    to encode the name and year released is odd, then an extra '\0' of padding sits between 
 * 				the year released and the data that follows
 * 
 * 				a two byte short storing the number of actors appearing in the film, padded with two additional
 * 		        bytes of zeros if needed (to make movie title + year released + numActors divisible by 4) 
 * 
 * 				an array of offsets into the actorFile. Each offset represents and actors (i.e. the array 
 * 				represents the cast of the movie
 */ 
 * 	
 * 
 */



const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

imdb::imdb(const string& directory)
{
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;
  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);

}

bool imdb::good() const
{
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}


/** Implementation note: nameCmp
 *  ----------------------------
 * nameCmp is the comparison function that is passed to bsearch to  
 * find an actor record
 * 
 * The function is tricky because we don't have a simple vector of 
 * strings that we're searching in. Instead of having two strings that 
 * that we can directly compare, we have a string and an integer offset
 * into the actorFile. We have to translate the offset into a string so we 
 * make a string-to-string comparison
 * 
 * @param vp1: we pass an ActorSearchPair as the first argument. The 
 *     		   ActorSearchPair is a struct with two fields. The first field 
 * 			   is a pointer to the actor's name. The second field is 
 * 			   a pointer to the actorFile. 
 * 
 * @param vp1: we pass an integer offset as the second argument. Combining 
 * 			   the integer offset with the actorFile pointer from the 
 *             ActorSearchPair, we can translate the offset into a string
 *             representing an actor's name, thus allowing a string-to-string 
 * 			   comparison
 */ 
int imdb::nameCmp(const void *vp1, const void *vp2) { 
 
  /* the first arg is a referece to the search pair struct
  *  we immediately cast to the type we know it is*/  
  ActorSearchPair *sp = (ActorSearchPair*)vp1; 
  /* we know pointers are 4 bytes and the 1st field of a search pair 
   * is a pointer. We hop over the pointer to get the name field
   */ 
  const char *name1 = sp->playerName;
  
  /* the second arg is a pointer to an integer offset 
   * we immediately cast to (int*) and dereference the offset
   */ 

  int offset = *(int*)vp2;

  /*using the offset we get a pointer to the actor record
   *the first element of the record is the actor name*/ 
  char *name2 = (char*)sp->actorFilePtr + offset*sizeof(char);
  
  int resStrcmp = strcmp(name1, name2); 
  
  return resStrcmp; 
}

/** Implementation note: getCredits
 * --------------------------------
 * getCredits populates a vectors with the movies an actor has appeared in
 * 
 * getCredits first step is to use binary search to find the actor record 
 * in the actorFile. 
 * 
 * The code is tricky because we're searching (i.e. walking down) an array of integer 
 * offsets rather than an array of strings (i.e. we're working with a string
 * and an integer offset that needs to be converted to a string). 
 * 
 * The translation of the offset is accomplished in the comparison function 
 * (and configuring the arguments that we pass to the comparison function cleverly).  
 * We pass the comparison function an ActorSearchPair, which has an actor's name 
 * and a pointer to the actorFile. The second argument is an integer offset. 
 * The comparison function then interprets those argument's and translates
 * the integer offset to a name to enable comparison. 
 */ 


bool imdb::getCredits(const string& player, vector<film>& films) const { 
  /* the first step is to use binary search to find the player record 
   * in the actorfile
   */ 


  /* binary search takes key, base, n, stride, and compareFunction
   * the first element of the data is a 4 byte integer 
   * Store away the number of actors.
   */
  int numActors = *(int*)actorFile; 
  
  /* We hop over numActors get to the start of the actor offsets 
   */ 
  void *actorsBase = (char*)actorFile + 1*sizeof(int);

  ActorSearchPair currPair; 
  currPair.actorFilePtr = actorFile; 
  const char *playerPtr = player.c_str(); 
  currPair.playerName = playerPtr; 
   

  void *actorOffsetPtr = bsearch(&currPair, actorsBase, numActors, sizeof(int),nameCmp); 
  
  if ( !actorOffsetPtr ) return false; 
 
  /** We have to do some arithetic to hop to the movie offsets we're interested in.
   * We have to hop char's representing the actor's name and the short representing
   * the movie year. The name is padded to be divisible by 2. The prefix (name + year) 
   * is padded to be divisible by 4. 
   */ 

  /* strlen does not include the terminating null character so we add 1*/  
  int nameLen = strlen(player.c_str()) + 1;  
  /* the value is always padded to make it an even number (strlen doesn't about padding)
   * if the length isn't even we increment by one for the padding we know must be there
   */ 
  if(nameLen % 2 != 0 )
    nameLen++;
  
   
  /*
    char *tempnameptr = (char*)malloc(nameLen*sizeof(char)); 
    strcpy(tempnameptr, (const char*)actorRecord); 
    string strtemp(tempnameptr); 
  */
  int currOffset = *(int*)actorOffsetPtr; 
  void* actorRecord = (char*)actorFile + currOffset*sizeof(char); 
  string strtemp = (char*)actorRecord;
  

  void* numMoviesPtr = (char*)actorRecord + nameLen*sizeof(char); 
  
  unsigned short numMovies = *(short*)numMoviesPtr; 
    
  int prefixLen = nameLen + 2; 
  if (prefixLen % 4 != 0) 
    /* the prefix (i.e. the name and numMovies must take up space divisible by 4
     * if the prefix is not divisible by 4, we increment prefix by 2 for the 
     * padding we know must be there */ 
    prefixLen += 2; 
 
  void *creditsBase = (char*)actorRecord + prefixLen*sizeof(char); 
  for (int i = 0; i < numMovies; i++){ 
    void *offsetPtr = (char*)creditsBase + i*sizeof(int); 
    int movieOffset = *(int*)offsetPtr; 
    void *moviePtr = (char*)movieFile + movieOffset*sizeof(char); 
    
    char *titlePtr = strdup((char*)moviePtr); 
    string movieTitle(titlePtr); 
    free(titlePtr); 
    
    /* adjust the length for the /0 padding not included in strlen*/ 
    int movieTitleLen = strlen(movieTitle.c_str()) + 1; 
    /* movie year is stored a 1 byte char. We calculate the position 
     * of the year, dereference, and cast to int (optional)*/ 
    int movieYear = *((char*)moviePtr + movieTitleLen*sizeof(char));
    film curr; 
    curr.title = movieTitle; curr.year = movieYear; 
    films.push_back(curr); 
  }			      

  return true; 
   
}

/** Implemenation note: movieCmp
 * -----------------------------
 * movieCmp is the comparison function that we pass to binary search to 
 * locate a movie record. 
 * 
 * Like nameCmp, movieCmp has to interpret and translate the arguments that
 * are passed to it. 
 * 
 * The first argument is the search term. The second argument is an integer offset
 * that represents the movie record in the movieFile. bSearch is actually searching
 * through the array of integer offsets. 
 * 
 * movieCmp extracts the movieFile pointer (which is passes as a field in the movieSearchPair)
 * and combines the movieFile pointer with the offset to get the movie name associated with 
 * that offset. 
 *
 * movieCmp is slightly more complicated than nameCmp because we can have duplicate movie names. 
 * In that instance, we use the year of release to distinguish between movies. To implement the 
 * additional functionality instead of passing a string for the first argument, we pass a film 
 * which has both a title and a year (i.e. the year of release). 
 *
 * @param vp1: we know a MovieSearchPair with fields for a pointer to a film and a pointer to 
 * 			   the movieFile is passed as the first argument. 
 * @param vp2: and integer offset representing a movie record. 
 */  
 


int imdb::movieCmp(const void *vp1, const void* vp2)
{ 
  MovieSearchPair *sp = (MovieSearchPair*)vp1; 
  const void *referenceFile = sp->movieFilePtr; 
  const film *tempFilm = sp->targetFilm; 
  string keyTitle = tempFilm->title; /*  *film.*title */
  const char *keyTitlePtr = keyTitle.c_str(); 
  int keyYear = tempFilm->year; 

  int currOffset = *(int*)vp2; 
  
  void *currRecordPtr = (char*)referenceFile + currOffset*sizeof(char); 
  int cmpTitleLen = strlen((char*)currRecordPtr)+1; 

  void *movieYearPtr = ((char*)currRecordPtr + cmpTitleLen*sizeof(char));
  int cmpYear = *(char*)movieYearPtr; 
  string temptitle = (char*)currRecordPtr; 
     
  int resStrcmp = strcmp(keyTitlePtr, (char*)currRecordPtr); 

  if (resStrcmp != 0) return resStrcmp; 
  
  if (keyYear != cmpYear) return keyYear - cmpYear; 
  
  return 0; 
}

/** Implementation note: getCast
 * -----------------------------
 * getCast takes a pointer to a film struct (defined in imdb-utils) and populates a
 * vector with the cast of that movie
 * 
 * getCast relies on binary search (using the movieCmp function defined above) to 
 * efficiently search the data file
 */ 

bool imdb::getCast(const film& movie, vector<string>& players) const { 

  MovieSearchPair currPair; 
  currPair.movieFilePtr = movieFile; 
  const film *currFilmPtr;
  currFilmPtr = &movie; 
  currPair.targetFilm = currFilmPtr; 
  
  int numMovies = *(int*)movieFile; 
  void *startMovieOffsets = (char*)movieFile + 1*sizeof(int); 
  
  void *movieOffsetPtr = bsearch(&currPair, startMovieOffsets, numMovies, sizeof(int), movieCmp); 
  
  if ( !movieOffsetPtr ) return false; 
  
  int currOffset = *(int*)movieOffsetPtr; 
  void *movieRecord = (char*)movieFile + currOffset*sizeof(char); 

  /* strlen excludes the terminating \0*/ 
  int movieTitleLen = strlen((char*)movieRecord) + 1; 
  

  /* a 1 byte short representing the year of the move sits after the title 
   * we check to see if the name + 1 byte is divisible by 2 
   */ 
  int partialPrefix = movieTitleLen + 1; 
  if (partialPrefix % 2 != 0) 
    partialPrefix++; 

  unsigned short numCast = *(short*)((char*)movieRecord + partialPrefix*sizeof(char)); 

   /* a 2 byte short representing the number of cast members 
   * sits after the partial prefix. If the partial prefix 
   * plus the 2 byte short is not divisible by 2, we
   * add 2 for the padding we know is there
   */ 
  int fullPrefix = partialPrefix + 2; 
  if ( fullPrefix % 4 != 0) 
    fullPrefix += 2; 

  void *offsetsBase = (char*)movieRecord + fullPrefix*sizeof(char); 

  for (int i = 0; i < numCast; i++) { 
    int numCastOffset = *(int*)((char*)offsetsBase + i*sizeof(int)); 
    void *currActorRecord = (char*)actorFile + numCastOffset*sizeof(char); 
    string currActor = (char*)currActorRecord; 
    players.push_back(currActor); 
  }

  return true; 
  
}

imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// ignore everything below... it's all UNIXy stuff in place to make a file look like
// an array of bytes in RAM.. 
const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info)
{
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info)
{
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
