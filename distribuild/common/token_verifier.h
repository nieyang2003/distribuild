
#include <string>
#include <unordered_set>

namespace distribuild {

class TokenVerifier {
 public:
  TokenVerifier() = default;
  explicit TokenVerifier(std::unordered_set<std::string> recognized_tokens);
  
  bool Verify(const std::string& token) const noexcept;

 private:
  std::unordered_set<std::string> recognized_tokens_;
};

std::unique_ptr<TokenVerifier> MakeTokenVerifier(const std::string& TokenStrs);

} // namespace distribuild