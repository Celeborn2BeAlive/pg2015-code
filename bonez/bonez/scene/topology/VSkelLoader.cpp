#include <fstream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "VSkelLoader.hpp"

namespace BnZ {

using namespace std;

/**
 * Read the 3D grid containing for each voxel the index of the nearest skeleton node.
 *
 * \param filepath The path of a PGM file containing the grid
 * \param out size Is renseigned with the size (width, height, depth) of the grid.
 *
 * \return An array containing the data
 */
CurvilinearSkeleton::GridType readPGM(const FilePath& filepath) {
    Vec3u size;
    ifstream in(filepath.c_str());

    if (!in) {
        throw runtime_error("Unable to open the file " + filepath.str() + ".");
    }

    std::string ext = filepath.ext();

    if (ext != "ppm" && ext != "pgm") {
        throw runtime_error("Invalid format for the file " + filepath.str() + 
            ". Recognized formats are .ppm / .pgm");
    }

    string line;
    getline(in, line, '\n');

    // Read file code
    if ((line.find("P9") == string::npos)
            && (line.find("P8") == string::npos)
            && (line.find("P6") == string::npos)
            && (line.find("P5") == string::npos)) {
        throw std::runtime_error("PPM / PGM invalid file (error in the code).");
    }

    // Skip commentaries
    do {
        getline(in, line, '\n');
    }
    while (line.at(0) == '#');

    istringstream iss(line);
    
    // Read the size of the grid
    iss >> size.x;
    iss >> size.y;
    iss >> size.z;
    size_t totalSize = size.x * size.y * size.z;
    getline(in, line, '\n');

    CurvilinearSkeleton::GridType grid(size.x, size.y, size.z);
    in.read(reinterpret_cast<char*>(grid.data()), totalSize * sizeof(GraphNodeIndex));

    return grid;
}

/**
 * An helper structure to read a curvilinear skeleton from disk.
 */
struct SkelVSKLReader {
    // Directory of input file
    FilePath path;
    
    // Current line
    char line[10000];

    // Buffers
    vector<Vec3f> positions; // Similar to a float array storing the skeleton node position
    Graph successors; // Store of successors
    vector<float> max_ball; // Similar to a float array storing the max ball size
    
    // Store of segmented successors. First integer is the indice of node in the global node list.
    // The rest are the successors indiced in the seg_succs list...
    // seg_succs[i][0] -> global index of i
    // seg_succs[i][j > 0] -> segmented index of the j-th successor of i
    Graph seg_succs;
    
    // Store of indice of segmented nodes in the original node list: seg_indices[global_index] = index dans seg_succs
    // The map is used by the method findPt to find the segmented index given the global index of a node
    map<GraphNodeIndex, GraphNodeIndex> seg_indices;

    FilePath grid_name_file; // Store the filename (pgm) of the grid
    Mat4f grid_transform; // Store the world to grid coordinates transform

    // Store the world to grid scale factor of the grid (usefull for maxball radius transformation)
    float worldToGridScale;

    /**
     * Helper functions to parse the file:
     */

    /*! Fill space at the end of the token with 0s. */
    static inline const char* trimEnd(const char* token) {
        size_t len = strlen(token);

        if (len == 0) {
            return token;
        }

        char* pe = (char*) (token + len - 1);

        while ((*pe == ' ' || *pe == '\t' || *pe == '\r') && pe >= token) {
            *pe-- = 0;
        }

        return token;
    }

    static inline bool isSep(const char c) {
        return (c == ' ') || (c == '\t');
    }

    /*! parse separator */
    static inline const char* parseSep(const char*& token) {
        size_t sep = strspn(token, " \t");

        if (!sep) {
            throw std::runtime_error("separator expected");
        }

        return token += sep;
    }

    /*! parse optional separator */
    static inline const char* parseSepOpt(const char*& token) {
        return token += strspn(token, " \t");
    }

    /*! Read size_t from a string */
    static inline size_t getsize_t(const char*& token) {
        token += strspn(token, " \t");
        size_t n = (size_t) atoi(token);
        token += strcspn(token, " \t\r");
        return n;
    }

    /*! Read int from a string */
    static inline int getNextInt(const char*& token) {
        //std::cerr<<"On a ("<<token<<") with '"<<strspn(token," \t")<<"'"<<std::endl;
        token += strspn(token, " \t");
        int sztmp = strcspn(token, " ,\t\r");
        //std::cerr<<"Find at "<<sztmp<<std::endl;
        char tmpint[30] = { '\0' };
        strncpy(tmpint, token, sztmp);
        //std::cerr<<"On a ("<<token<<")"<<std::endl;
        int n = (int) atoi(tmpint);
        //std::cerr<<"Generate "<<n<<std::endl;
        token += sztmp;
        //std::cerr<<"We still have "<<strspn(token, " ,\t\r")<<" caracters to consume"<<std::endl;
        token += strspn(token, " ,\t\r");
        //std::cerr<<"Token final ("<<token<<")"<<std::endl;
        return n;
    }

    /*! Read float from a string */
    static inline float getFloat(const char*& token) {
        token += strspn(token, " \t");
        float n = (float) atof(token);
        token += strcspn(token, " \t\r");
        return n;
    }

    /*! Read Vec2f from a string */
    static inline Vec2f getVec2f(const char*& token) {
        float x = getFloat(token);
        float y = getFloat(token);
        return Vec2f(x, y);
    }

    /*! Read Vec3f from a string */
    static inline Vec3f getVec3f(const char*& token) {
        float x = getFloat(token);
        float y = getFloat(token);
        float z = getFloat(token);
        return Vec3f(x, y, z);
    }

    /*! Read string from a string */
    static inline const char* getWord(const char*& token, int& size_word) {
        char* result;
        const char* ptr = strpbrk(token, " \t");

        if (ptr == NULL) {
            result = new char[1];
            result[0] = '\0';
            size_word = 0;
            return result;
        }

        size_word = strlen(token) - strlen(ptr);
        result = new char[size_word + 1];
        strncpy(result, token, size_word);
        result[size_word] = '\0';
        token = parseSep(ptr);
        return result;
    }

    const char* getMultilineFromStream(std::ifstream& cin) {
        /* load next multiline */
        char* pline = line;

        while (true) {
            cin.getline(pline, sizeof(line) - (pline - line) - 16, '\n');
            auto last = strlen(pline) - 1;

            if (last < 0 || pline[last] != '\\') {
                break;
            }

            pline += last;
            *pline++ = ' ';
        }

        return trimEnd(line + strspn(line, " \t"));
    }

    void handleCoordinateNode(const char* token, std::ifstream& cin) {
        const char* data;
        Vec3f onept;

        if (cin.peek() != -1) {
            if (token[0] == '[') {
                // In this case we have an array of coordinates
                //std::cerr<<"Array of coordinates..."<<std::endl;
                while (true) {
                    data = getMultilineFromStream(cin);

                    if (data[0] == ']') {
                        //std::cerr<<"Fin array"<<std::endl;
                        break;
                    }

                    onept = getVec3f(data);
                    positions.push_back(onept);
                }

                //std::cerr<<"Size of vector : "<<v.size()<<std::endl;
                successors.clear();
                successors.resize(positions.size());
            }
            else {
                std::cerr << "Error in property of point node : property is "
                          << token << std::endl;
            }
        }

        // 1 seule propriete doit etre présente...
    }

    // charge les successeurs
    void handleIndexedLineSet(const char* token, std::ifstream& cin) {
        const char* line;
        int l[2];
        int index = 0;

        //std::cerr<<"HAaaa  : "<<token<<std::endl;
        if (token[0] == '[') {
            line = getMultilineFromStream(cin);
            //std::cerr<<"Get a new line : "<<line<<std::endl;
            // Récuperer des series de 2 entiers
            l[index] = getNextInt(line);

            while ((cin.peek() != -1) && (line[0] != ']')) {
                index = 1 - index;
                l[index] = getNextInt(line);

                if ((l[index] != -1) && (l[1 - index] != -1)) {
                    // We have to link l[index] et l[1-index]
                    successors[l[index]].push_back(l[1 - index]);
                    //std::cerr<<"Linking "<<l[index]<<" to "<<l[1-index]<<std::endl;
                    successors[l[1 - index]].push_back(l[index]);
                    //std::cerr<<"Linking "<<l[1-index]<<" to "<<l[index]<<std::endl;
                }

                if ((line[0] == '\n') || (line[0] == '\0')) {
                    line = getMultilineFromStream(cin);
                }
            }
        }
    }

    // Recherche l'indice du noeud key dans seg_succ: on le retrouve dans la map
    GraphNodeIndex findPt(size_t key) {
        std::map<GraphNodeIndex, GraphNodeIndex>::iterator it = seg_indices.find(key);

        if (it == seg_indices.end()) {
            // Creation of a new element in seg_succs and also in seg_indices
            std::vector<GraphNodeIndex> newelt;
            newelt.push_back(key); // Le premier element est l'indice global du noeud
            seg_succs.push_back(newelt);
            seg_indices.insert(
                std::pair<GraphNodeIndex, GraphNodeIndex>(key, seg_succs.size() - 1)); // Map l'indice global sur l'indice dans le tableau des succ
            return seg_succs.size() - 1;
        }
        else {
            return it->second;
        }
    }

    void handleSegmentedLineSet(const char* token, std::ifstream& cin) {
        const char* line;
        int l[2];
        int index = 0;
        GraphNodeIndex id1, id2;

        //std::cerr<<"HAaaa  : "<<token<<std::endl;
        if (token[0] == '[') {
            line = getMultilineFromStream(cin);
            //std::cerr<<"Get a new line : "<<line<<std::endl;
            // Récuperer des series de 2 entiers
            l[index] = getNextInt(line);

            while ((cin.peek() != -1) && (line[0] != ']')) {
                index = 1 - index;
                l[index] = getNextInt(line);

                if ((l[index] != -1) && (l[1 - index] != -1)) {
                    // We have to link l[index] et l[1-index]
                    id1 = findPt(l[index]);
                    id2 = findPt(l[1 - index]);
                    seg_succs[id1].push_back(id2);
                    //std::cerr<<"Linking "<<l[index]<<" to "<<l[1-index]<<std::endl;
                    seg_succs[id2].push_back(id1);
                    //std::cerr<<"Linking "<<l[1-index]<<" to "<<l[index]<<std::endl;
                }

                if ((line[0] == '\n') || (line[0] == '\0')) {
                    line = getMultilineFromStream(cin);
                }
            }
        }
    }

    void handleMaxball(const char* token, std::ifstream& cin) {
        const char* data;
        float one_ball;

        if (cin.peek() != -1) {
            if (token[0] == '[') {
                // In this case we have an array of coordinates
                //std::cerr<<"Array of coordinates..."<<std::endl;
                while (true) {
                    data = getMultilineFromStream(cin);

                    if (data[0] == ']') {
                        //std::cerr<<"Fin array"<<std::endl;
                        break;
                    }

                    one_ball = getFloat(data);
                    max_ball.push_back(one_ball);
                }
            }
            else {
                std::cerr << "Error in property of point node : property is "
                          << token << std::endl;
            }
        }

        // 1 seule propriete doit etre présente...
    }

    SkelVSKLReader(const FilePath& fileName) {
        /* open file */
        std::ifstream cin;
        cin.open(fileName.c_str());

        if (!cin.is_open()) {
            throw std::runtime_error("Cannot open " + fileName.str());
        }

        /*! generate default skeleton */
        successors.clear();
        seg_succs.clear();
        positions.clear();
        max_ball.clear();
        /*! parse file */
        std::cerr << "Beginning to parse skeleton file " << fileName << "..."
                  << std::endl;
        memset(line, 0, sizeof(line));
        char* headline = line;
        cin.getline(headline, sizeof(line) - (headline - line) - 16, '\n');

        if (strncmp(headline, "#SkelFormat", 11) != 0) {
            throw std::runtime_error("Error in loading file : header is not #SkelFormat");
        }

        while (cin.peek() != -1) {
            const char* token = getMultilineFromStream(cin);

            /*! parse empty line */
            if (token[0] == 0) {
                continue;
            }

            /*! parse end of a node*/
            // No need
            /*! parse point */
            if (!strncmp(token, "point", 5) && isSep(token[5])) {
                //std::cerr<<"READING point property"<<std::endl;
                handleCoordinateNode(parseSep(token += 5), cin);
            }

            /*! parse line */
            if (!strncmp(token, "line", 4) && isSep(token[4])) {
                //std::cerr<<"READING line property"<<std::endl;
                handleIndexedLineSet(parseSep(token += 4), cin);
            }

            /*! parse seg_line */
            if (!strncmp(token, "seg_line", 8) && isSep(token[8])) {
                //std::cerr<<"READING seg_line property"<<std::endl;
                handleSegmentedLineSet(parseSep(token += 8), cin);
            }

            /*! parse max ball */
            if (!strncmp(token, "sizemaxball", 11) && isSep(token[11])) {
                //std::cerr<<"READING seg_line property"<<std::endl;
                handleMaxball(parseSep(token += 11), cin);
            }

            /*! parse grid */
            if (!strncmp(token, "grid", 4) && isSep(token[4])) {
                //std::cerr<<"READING grid property"<<std::endl;
                token += 4;
                parseSep(token);
                int size;
                const char* fname = getWord(token, size);
                grid_name_file = path + FilePath(fname);
                Vec3f translate = getVec3f(token);
                worldToGridScale = getFloat(token);
                grid_transform = Mat4f(Vec4f(0.0, 0.0, worldToGridScale, 0),
                                               Vec4f(worldToGridScale, 0.0, 0.0, 0), Vec4f(0.0, worldToGridScale, 0.0, 0),
                                               Vec4f(worldToGridScale * translate.y, worldToGridScale * translate.z,
                                                     worldToGridScale * translate.x, 1));
            }

            /*! depending on which node we are we can now delete the whole line... */
            // ignore unknown stuff
        }

        std::cerr << "WE STORE : " << positions.size()
                  << " original nodes with a segmented line of " << seg_succs.size()
                  << " nodes " << std::endl;
        std::cerr << "LOADING MAX BALL DONE with " << max_ball.size()
                  << std::endl;
        std::cerr << "End parse file ..." << std::endl;
    }

    /*! handles relative indices and starts indexing from 0 */
    int fix_v(int index) {
        return index > 0 ? index - 1 : index == 0 ? 0 : (int) positions.size() + index;
    }
};

CurvilinearSkeleton VSkelLoader::loadCurvilinearSkeleton(const FilePath& filepath) {
    SkelVSKLReader reader(m_BasePath + filepath);
    auto skelGrid = readPGM(m_BasePath + filepath.directory() + FilePath(reader.grid_name_file));

    CurvilinearSkeleton skeleton;

    std::cerr << "World to grid scale: " << reader.worldToGridScale << std::endl;
    std::cerr << "Grid resolution: " << skelGrid.width() << ", " << skelGrid.height() << ", " << skelGrid.depth() << std::endl;

    for (GraphNodeIndex i = 0; i < reader.positions.size(); ++i) {
        float worldMaxBall = reader.max_ball[i] / (reader.worldToGridScale * reader.worldToGridScale);
        
        skeleton.addNode(reader.positions[i], sqrt(worldMaxBall));
    }

    skeleton.setGraph(reader.successors);
    skeleton.setGrid(skelGrid);

    skeleton.setWorldToGrid(reader.grid_transform, reader.worldToGridScale);

    return skeleton;
}

}
