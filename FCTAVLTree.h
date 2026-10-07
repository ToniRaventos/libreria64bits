//---------------------------------------------------------------------------
                         //---------------------------------------------------------------------------
#ifndef FCTAVLTreeH
#define FCTAVLTreeH

#include <string>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <cstddef>

class CTSTD;

//---------------------------------------------------------------------------
// Nodo del árbol AVL
//---------------------------------------------------------------------------
class AVLNode
{
public:
    std::string key;
    AVLNode*    left;
    AVLNode*    right;
    int         height;
    int         index;
    CTSTD*      data;
    bool        is_free_data;

    explicit AVLNode(const std::string& k);
    ~AVLNode();

    AVLNode(const AVLNode&)            = delete;
    AVLNode& operator=(const AVLNode&) = delete;

    void    SetData(CTSTD* obj, bool borrar = true);
    CTSTD*  GetData() const;
    void    ClearData();
};

//---------------------------------------------------------------------------
// Árbol AVL
//---------------------------------------------------------------------------
class CTAVLTree
{
public:
    CTAVLTree();
    ~CTAVLTree();

    CTAVLTree(const CTAVLTree&)            = delete;
    CTAVLTree& operator=(const CTAVLTree&) = delete;

    // --- API original (compatibilidad) ---
    void      Clear();
    AVLNode*  Add(const std::string& key, int idx = 0);
    void      Insert(std::string key);
    void      Remove(std::string key);
    void      Remove(int idx);
    bool      Search(std::string key) const;
    void      inOrder() const;
    void      preOrder() const;
    void      postOrder() const;
    AVLNode*  Find(const std::string& key) const;
    int       Count() const;
    AVLNode*  Item(int idx) const;
    AVLNode*  GetByIndex(int idx) const;

    void      TraverseNodes(std::function<void(AVLNode*)> callback);

private:
    AVLNode*  root;
    int       indexCount;

    // Índice secundario: index -> nodo. Reemplaza al antiguo IndexToNode comentado.
    std::unordered_map<int, AVLNode*> IndexToNode;

    mutable std::mutex treeMutex;   // protege todas las operaciones

    // --- Helpers AVL ---
    static int  getHeight(const AVLNode* n);
    static int  getBalanceFactor(const AVLNode* n);
    static AVLNode* rotateRight(AVLNode* y);
    static AVLNode* rotateLeft (AVLNode* x);

    AVLNode* insertNode(AVLNode* node, const std::string& key, AVLNode*& newNode);
    AVLNode* deleteNode(AVLNode* node, const std::string& key);

    static AVLNode* minValueNode(AVLNode* node);
    static AVLNode* findNode(const AVLNode* node, const std::string& key);

    static void clearTree(AVLNode* node);
    static void inOrderTraversal (const AVLNode* node);
    static void preOrderTraversal(const AVLNode* node);
    static void postOrderTraversal(const AVLNode* node);

    void TraverseNodesHelper(AVLNode* node, std::function<void(AVLNode*)>& callback);

    // Reconstruye IndexToNode desde cero tras un borrado o Clear.
    void RebuildIndex();
    // Recorre el subárbol y vuelca los nodos en un vector, en orden.
    static void Collect(AVLNode* node, std::vector<AVLNode*>& out);
};

//---------------------------------------------------------------------------
// Utilidad de ceros a la izquierda (antes en el .cpp)
//---------------------------------------------------------------------------
std::string Zeros(int number, int width);

// Prueba de estrés (opcional). Devuelve 0 si todo va bien.
int Prova_AVLTREE();

#endif
#ifndef FCTAVLTreeH
#define FCTAVLTreeH

#include <unordered_map>
#include <iostream>
 #include <mutex>
class CTSTD;
//---------------------------------------------------------------------------
class AVLNode {
public:
	std::string key;
	AVLNode* left;
	AVLNode* right;
	int height;
	int index;
	CTSTD *data;
	bool is_free_data;
	AVLNode(std::string k) ;
	~AVLNode();
	void SetData(CTSTD *obj, bool borrar=true);
	CTSTD *GetData();
      void ClearData();
};

class CTAVLTree
{
 public:
  //	std::unordered_map<int, AVLNode*> IndexToNode;
 private:

	 std::mutex treeMutex; // Mutex per protegir les operacions de l'arbre
	int indexCount;

	AVLNode* root;
	int getHeight(AVLNode* node);
	int getBalanceFactor(AVLNode* node);
	AVLNode* rotateRight(AVLNode* y);
	AVLNode* rotateLeft(AVLNode* x);
	AVLNode* addNode(AVLNode* node, const std::string& key);
	AVLNode* insertNode(AVLNode* node, const std::string& key, AVLNode*& newNode);
	AVLNode* minValueNode(AVLNode* node);
	AVLNode* deleteNode(AVLNode* node, std::string key);
	void inOrderTraversal(AVLNode* node);
	void preOrderTraversal(AVLNode* node);
	void postOrderTraversal(AVLNode* node);
	void clearTree(AVLNode* node);
	AVLNode* findNode(AVLNode* node, const std::string& key);


 public:
  CTAVLTree();

  ~CTAVLTree();
  void Clear();
  AVLNode* Add(const std::string& key,int idx=0);
  void Insert(std::string key);
  void Remove(std::string key);
  void Remove(int idx);
  bool Search(std::string key);
  void inOrder();
  void preOrder();
  void postOrder();
  AVLNode* Find(const std::string& key);
  int Count();
  AVLNode *Item(int idx);
	AVLNode* GetByIndex(int idx);
  void TraverseNodes(std::function<void(AVLNode*)> callback);
  void TraverseNodesHelper(AVLNode* node, std::function<void(AVLNode*)> callback);


};

int Prova_AVLTREE();


#endif
