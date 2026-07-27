#include "UiStrings.h"

namespace ui_strings {
namespace {
UiLanguage currentLanguage = UiLanguage::English;

const Text english = {
    "Library", "Scanning directory...", "No folders or EPUB files found",
    "..  Back", "[Folder] ", "[EPUB] ",
    "List limited to protect memory", "EPUB selected", "Page",
    "Reading menu", "Back to library", "Table of contents", "Restart book",
    "Close menu", "Zoom", "Font", "Chapter", "Approximate progress",
    "Restart reading?", "The saved position will be erased.",
    "Yes, return to the beginning", "Cancel", "Language", "Invalid EPUB",
    "Tap to return", "Enter sleep mode", "Sleep Mode",
    "Use the font lever to wake"};

const Text portuguese = {
    "Biblioteca", "Lendo diretorio...", "Nenhuma pasta ou EPUB encontrado",
    "..  Voltar", "[Pasta] ", "[EPUB] ",
    "Lista limitada para proteger a memoria", "EPUB selecionado", "Pagina",
    "Menu de leitura", "Voltar a biblioteca", "Sumario", "Reiniciar livro",
    "Fechar menu", "Zoom", "Fonte", "Capitulo", "Progresso aproximado",
    "Reiniciar leitura?", "A posicao salva sera apagada.",
    "Sim, voltar ao inicio", "Cancelar", "Idioma", "EPUB invalido",
    "Toque para voltar", "Entrar em modo de espera",
    "Modo de espera (Sleep Mode)", "Use a alavanca de fonte para acordar"};
}  // namespace

const Text& get() { return currentLanguage == UiLanguage::Portuguese ? portuguese : english; }
UiLanguage language() { return currentLanguage; }
void setLanguage(UiLanguage language) { currentLanguage = language; }
const char* languageName() {
  return currentLanguage == UiLanguage::Portuguese ? "Português" : "English";
}
}  // namespace ui_strings
