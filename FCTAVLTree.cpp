//---------------------------------------------------------------------------
#include "FCTAVLTree.h"
#include "FCTLib.h"          // sólo por CTSTD (TpClase, etc.)

#include <algorithm>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// AVLNode
//---------------------------------------------------------------------------
AVLNode::AVLNode(const std::string& k)
    : key(k), left(nullptr), right(nullptr), height(1),
      index(0), data(nullptr), is_free_data(false)
{
}

AVLNode::~AVLNode()
{
    ClearData();
}

void AVLNode::ClearData()
{
    if (is_free_data && data != nullptr)
        delete data;

    data = nullptr;
    is_free_data = false;
}

void AVLNode::SetData(CTSTD* obj, bool borrar)
{
    // Si había un data previo marcado como propio, liberarlo antes de sustituir.
    ClearData();
    data         = obj;
    is_free_data = borrar;
}

CTSTD* AVLNode::GetData() const
{
    return data;
}

//---------------------------------------------------------------------------
// CTAVLTree — ciclo de vida
//---------------------------------------------------------------------------
CTAVLTree::CTAVLTree()
    : root(nullptr), indexCount(0)
{
}

CTAVLTree::~CTAVLTree()
{
    Clear();
}

//---------------------------------------------------------------------------
// Helpers AVL (static)
//---------------------------------------------------------------------------
int CTAVLTree::getHeight(const AVLNode* n)
{
    return n ? n->height : 0;
}

int CTAVLTree::getBalanceFactor(const AVLNode* n)
{
    return n ? getHeight(n->left) - getHeight(n->right) : 0;
}

AVLNode* CTAVLTree::rotateRight(AVLNode* y)
{
    AVLNode* x  = y->left;
    AVLNode* T2 = x->right;

    x->right = y;
    y->left  = T2;

    y->height = 1 + std::max(getHeight(y->left), getHeight(y->right));
    x->height = 1 + std::max(getHeight(x->left), getHeight(x->right));
    return x;
}

AVLNode* CTAVLTree::rotateLeft(AVLNode* x)
{
    AVLNode* y  = x->right;
    AVLNode* T2 = y->left;

    y->left  = x;
    x->right = T2;

    x->height = 1 + std::max(getHeight(x->left), getHeight(x->right));
    y->height = 1 + std::max(getHeight(y->left), getHeight(y->right));
    return y;
}

AVLNode* CTAVLTree::minValueNode(AVLNode* node)
{
    AVLNode* cur = node;
    while (cur && cur->left) cur = cur->left;
    return cur;
}

AVLNode* CTAVLTree::findNode(const AVLNode* node, const std::string& key)
{
    while (node)
    {
        if      (key == node->key) return const_cast<AVLNode*>(node);
        else if (key <  node->key) node = node->left;
        else                       node = node->right;
    }
    return nullptr;
}

//---------------------------------------------------------------------------
// Inserción
//---------------------------------------------------------------------------
AVLNode* CTAVLTree::insertNode(AVLNode* node, const std::string& key, AVLNode*& newNode)
{
    if (node == nullptr)
    {
        newNode = new AVLNode(key);
        return newNode;
    }

    if (key < node->key)
        node->left  = insertNode(node->left,  key, newNode);
    else if (key > node->key)
        node->right = insertNode(node->right, key, newNode);
    else
        return node;   // duplicado: no insertar

    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));

    int balance = getBalanceFactor(node);

    if (balance >  1 && key < node->left->key)  return rotateRight(node);
    if (balance < -1 && key > node->right->key) return rotateLeft(node);

    if (balance >  1 && key > node->left->key)
    {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }
    if (balance < -1 && key < node->right->key)
    {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }
    return node;
}

//---------------------------------------------------------------------------
// Borrado
//---------------------------------------------------------------------------
AVLNode* CTAVLTree::deleteNode(AVLNode* node, const std::string& key)
{
    if (node == nullptr) return node;

    if (key < node->key)
    {
        node->left = deleteNode(node->left, key);
    }
    else if (key > node->key)
    {
        node->right = deleteNode(node->right, key);
    }
    else
    {
        // Nodo a borrar
        if (node->left == nullptr || node->right == nullptr)
        {
            AVLNode* temp = node->left ? node->left : node->right;
            if (temp == nullptr)
            {
                // Sin hijos: borrar el nodo
                delete node;
                return nullptr;
            }
            // Un hijo: copiar contenido del hijo y borrar el hijo.
            // Importante: liberar el data propio ANTES de sobreescribir el nodo.
            node->ClearData();
            node->key          = temp->key;
            node->data         = temp->data;
            node->is_free_data = temp->is_free_data;
            node->index        = temp->index;
            // Anular el data del temp para que su destructor no lo libere.
            temp->data         = nullptr;
            temp->is_free_data = false;
            delete temp;
        }
        else
        {
            // Dos hijos: sustituir por el sucesor in-order.
            AVLNode* temp = minValueNode(node->right);
            node->ClearData();
            node->key          = temp->key;
            node->data         = temp->data;
            node->is_free_data = temp->is_free_data;
            node->index        = temp->index;
            temp->data         = nullptr;
            temp->is_free_data = false;
            node->right = deleteNode(node->right, temp->key);
        }
    }

    if (node == nullptr) return node;

    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
    int balance = getBalanceFactor(node);

    if (balance >  1 && getBalanceFactor(node->left)  >= 0) return rotateRight(node);
    if (balance >  1 && getBalanceFactor(node->left)  <  0)
    {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }
    if (balance < -1 && getBalanceFactor(node->right) <= 0) return rotateLeft(node);
    if (balance < -1 && getBalanceFactor(node->right) >  0)
    {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }
    return node;
}

//---------------------------------------------------------------------------
// Recorridos
//---------------------------------------------------------------------------
void CTAVLTree::inOrderTraversal(const AVLNode* node)
{
    if (!node) return;
    inOrderTraversal(node->left);
    // std::cout << node->key << '\n';
    inOrderTraversal(node->right);
}

void CTAVLTree::preOrderTraversal(const AVLNode* node)
{
    if (!node) return;
    // std::cout << node->key << ' ';
    preOrderTraversal(node->left);
    preOrderTraversal(node->right);
}

void CTAVLTree::postOrderTraversal(const AVLNode* node)
{
    if (!node) return;
    postOrderTraversal(node->left);
    postOrderTraversal(node->right);
    // std::cout << node->key << ' ';
}

void CTAVLTree::clearTree(AVLNode* node)
{
    if (!node) return;
    clearTree(node->left);
    clearTree(node->right);
    delete node;
}

//---------------------------------------------------------------------------
// Índice secundario
//---------------------------------------------------------------------------
void CTAVLTree::Collect(AVLNode* node, std::vector<AVLNode*>& out)
{
    if (!node) return;
    Collect(node->left, out);
    out.push_back(node);
    Collect(node->right, out);
}

void CTAVLTree::RebuildIndex()
{
    IndexToNode.clear();
    std::vector<AVLNode*> nodes;
    Collect(root, nodes);

    // Reasignar índices compactos 0..N-1 en orden in-order.
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i]->index        = static_cast<int>(i);
        IndexToNode[nodes[i]->index] = nodes[i];
    }
    indexCount = static_cast<int>(nodes.size());
}

//---------------------------------------------------------------------------
// API pública
//---------------------------------------------------------------------------
void CTAVLTree::Clear()
{
    std::lock_guard<std::mutex> lock(treeMutex);
    clearTree(root);
    root = nullptr;
    indexCount = 0;
    IndexToNode.clear();
}

void CTAVLTree::Insert(std::string key)
{
    std::lock_guard<std::mutex> lock(treeMutex);

    AVLNode* existing = findNode(root, key);
    if (existing) return;               // ya existe, no hacemos nada

    AVLNode* newNode = nullptr;
    root = insertNode(root, key, newNode);

    if (newNode)
    {
        // Insertar en el índice secundario
        newNode->index = indexCount++;
        IndexToNode[newNode->index] = newNode;
    }
}

AVLNode* CTAVLTree::Add(const std::string& key, int idx)
{
    std::lock_guard<std::mutex> lock(treeMutex);

    AVLNode* node = findNode(root, key);
    if (node)
        return node;                    // ya existe, devolverlo

    AVLNode* newNode = nullptr;
    root = insertNode(root, key, newNode);

    if (newNode)
    {
        if (idx > 0)
        {
            newNode->index = idx;
            indexCount = std::max(indexCount, idx + 1);
        }
        else
        {
            newNode->index = indexCount++;
        }
        IndexToNode[newNode->index] = newNode;
    }
    return newNode;
}

void CTAVLTree::Remove(std::string key)
{
    std::lock_guard<std::mutex> lock(treeMutex);

    AVLNode* node = findNode(root, key);
    if (!node) return;

    int idx = node->index;
    root = deleteNode(root, key);
    IndexToNode.erase(idx);
}

void CTAVLTree::Remove(int idx)
{
    std::lock_guard<std::mutex> lock(treeMutex);

    auto it = IndexToNode.find(idx);
    if (it == IndexToNode.end()) return;

    AVLNode* node = it->second;
    if (!node) return;

    std::string key = node->key;
    IndexToNode.erase(it);
    root = deleteNode(root, key);
}

bool CTAVLTree::Search(std::string key) const
{
    std::lock_guard<std::mutex> lock(treeMutex);
    return findNode(root, key) != nullptr;
}

AVLNode* CTAVLTree::Find(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(treeMutex);
    return findNode(root, key);
}

AVLNode* CTAVLTree::Item(int idx) const
{
    return GetByIndex(idx);
}

AVLNode* CTAVLTree::GetByIndex(int idx) const
{
    std::lock_guard<std::mutex> lock(treeMutex);
    auto it = IndexToNode.find(idx);
    return (it != IndexToNode.end()) ? it->second : nullptr;
}

int CTAVLTree::Count() const
{
    std::lock_guard<std::mutex> lock(treeMutex);
    return static_cast<int>(IndexToNode.size());
}

void CTAVLTree::inOrder() const
{
    std::lock_guard<std::mutex> lock(treeMutex);
    inOrderTraversal(root);
    std::cout << std::endl;
}

void CTAVLTree::preOrder() const
{
    std::lock_guard<std::mutex> lock(treeMutex);
    preOrderTraversal(root);
    std::cout << std::endl;
}

void CTAVLTree::postOrder() const
{
    std::lock_guard<std::mutex> lock(treeMutex);
    postOrderTraversal(root);
    std::cout << std::endl;
}

//---------------------------------------------------------------------------
// Recorrido con callback
//---------------------------------------------------------------------------
void CTAVLTree::TraverseNodesHelper(AVLNode* node, std::function<void(AVLNode*)>& callback)
{
    if (!node) return;
    TraverseNodesHelper(node->left,  callback);
    callback(node);
    TraverseNodesHelper(node->right, callback);
}

void CTAVLTree::TraverseNodes(std::function<void(AVLNode*)> callback)
{
    std::lock_guard<std::mutex> lock(treeMutex);
    TraverseNodesHelper(root, callback);
}

//---------------------------------------------------------------------------
// Utilidad
//---------------------------------------------------------------------------
std::string Zeros(int number, int width)
{
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << number;
    return oss.str();
}

//---------------------------------------------------------------------------
// Prueba de estrés
//---------------------------------------------------------------------------
int Prova_AVLTREE()
{
    CTAVLTree tree;

    // Inserción: 5 millones
    for (int a = 0; a < 5000000; ++a)
    {
        CTSTD* BB = new CTSTD;
        BB->TpClase = (a + 1) * 10;

        AVLNode* HH = tree.Add("Key_" + Zeros(a + 1, 7), a);
        if (HH) HH->SetData(BB, true);
        else    delete BB;
    }

    std::cout << "Insertados: " << tree.Count() << std::endl;

    // Borrado de los impares
    for (int a = 0; a < 5000000; ++a)
    {
        if (a % 2)
            tree.Remove("Key_" + Zeros(a + 1, 7));
    }

    std::cout << "Tras borrado: " << tree.Count() << std::endl;

    // Comprobación: quedan los pares
    int esperados = 0;
    for (int a = 0; a < 5000000; ++a)
        if (a % 2 == 0) ++esperados;

    std::cout << "Esperados: " << esperados << std::endl;

    return (tree.Count() == esperados) ? 0 : 1;
}
