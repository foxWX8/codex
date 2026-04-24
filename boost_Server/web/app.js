const typing = document.querySelector(".typing");

if (typing) {
    const text = typing.dataset.text || typing.textContent;
    typing.textContent = "";

    let index = 0;
    const timer = window.setInterval(() => {
        typing.textContent += text[index] || "";
        index += 1;

        if (index > text.length) {
            window.clearInterval(timer);
        }
    }, 70);
}

const progressBar = document.createElement("div");
progressBar.className = "reading-progress";
document.body.prepend(progressBar);

const updateReadingProgress = () => {
    const scrollableHeight = document.documentElement.scrollHeight - window.innerHeight;
    const progress = scrollableHeight > 0 ? window.scrollY / scrollableHeight : 0;
    progressBar.style.transform = `scaleX(${Math.min(Math.max(progress, 0), 1)})`;
};

window.addEventListener("scroll", updateReadingProgress, { passive: true });
window.addEventListener("resize", updateReadingProgress);
updateReadingProgress();

const getStoredUser = () => ({
    username: window.localStorage.getItem("blog_username") || "",
    token: window.localStorage.getItem("blog_token") || ""
});

const updateAuthNav = () => {
    const { username, token } = getStoredUser();
    document.querySelectorAll('a[href="/login.html"]').forEach((link) => {
        if (username && token) {
            link.textContent = username;
            link.classList.add("user-link");
            link.setAttribute("title", "已登录");
        }
        else {
            link.textContent = "登录";
            link.classList.remove("user-link");
            link.removeAttribute("title");
        }
    });
};

updateAuthNav();

const searchForms = document.querySelectorAll(".nav-search");
const normalizeSearchText = (value) => value.trim().toLowerCase();

searchForms.forEach((form) => {
    const input = form.querySelector('input[name="q"]');
    const params = new URLSearchParams(window.location.search);
    const initialQuery = params.get("q") || "";

    if (input && initialQuery) {
        input.value = initialQuery;
    }

    form.addEventListener("submit", (event) => {
        event.preventDefault();
        const query = input ? input.value.trim() : "";
        const searchTarget = query
            ? `/?q=${encodeURIComponent(query)}#articles`
            : "/#articles";
        window.location.href = searchTarget;
    });
});

const articleList = document.querySelector("#articles > section");
const articleCards = document.querySelectorAll(".article-card");

if (articleList && articleCards.length > 0) {
    const noResults = document.createElement("div");
    noResults.className = "search-empty";
    noResults.textContent = "没有找到匹配的文章，换个关键词试试。";
    noResults.hidden = true;
    articleList.append(noResults);

    const applyArticleSearch = () => {
        const query = normalizeSearchText(new URLSearchParams(window.location.search).get("q") || "");
        let visibleCount = 0;

        articleCards.forEach((card) => {
            const haystack = normalizeSearchText(`${card.textContent} ${card.dataset.search || ""}`);
            const matched = !query || haystack.includes(query);
            card.hidden = !matched;
            if (matched) {
                visibleCount += 1;
            }
        });

        noResults.hidden = visibleCount > 0;

        if (query && window.location.hash !== "#articles") {
            window.location.hash = "articles";
        }
    };

    applyArticleSearch();
}

document.querySelectorAll(".article-card").forEach((card, cardIndex) => {
    const articleBody = card.querySelector(".article-body");
    if (!articleBody) {
        return;
    }

    const targetHref = card.dataset.href;
    if (targetHref) {
        const titleLink = card.querySelector(".article-body h2 a");
        if (titleLink && titleLink.getAttribute("href") === "#") {
            titleLink.href = targetHref;
        }

        card.setAttribute("tabindex", "0");
        card.setAttribute("role", "link");

        const openArticle = () => {
            window.location.href = targetHref;
        };

        card.addEventListener("click", (event) => {
            if (event.target.closest("a, button")) {
                return;
            }
            openArticle();
        });

        card.addEventListener("keydown", (event) => {
            if (event.key === "Enter") {
                openArticle();
            }
        });
    }

    const actionRow = document.createElement("div");
    actionRow.className = "article-actions";

    const likeButton = document.createElement("button");
    likeButton.type = "button";
    likeButton.className = "like-btn";
    likeButton.setAttribute("aria-pressed", "false");
    likeButton.textContent = `Like ${cardIndex + 1}`;

    const hint = document.createElement("span");
    hint.className = "article-hint";
    hint.textContent = "Click to mark";

    likeButton.addEventListener("click", () => {
        const liked = likeButton.getAttribute("aria-pressed") === "true";
        likeButton.setAttribute("aria-pressed", String(!liked));
        likeButton.textContent = liked ? `Like ${cardIndex + 1}` : "Liked";
        hint.textContent = liked ? "Click to mark" : "Saved locally in this page";
        card.classList.toggle("is-liked", !liked);
    });

    actionRow.append(likeButton, hint);
    articleBody.append(actionRow);
});

const backTopButton = document.createElement("button");
backTopButton.type = "button";
backTopButton.className = "back-top";
backTopButton.setAttribute("aria-label", "Back to top");
backTopButton.textContent = "^";
document.body.append(backTopButton);

const updateBackTopVisibility = () => {
    backTopButton.classList.toggle("is-visible", window.scrollY > 420);
};

backTopButton.addEventListener("click", () => {
    window.scrollTo({ top: 0, behavior: "smooth" });
});

window.addEventListener("scroll", updateBackTopVisibility, { passive: true });
updateBackTopVisibility();

const authForm = document.querySelector("[data-auth-form]");
const authTabs = document.querySelectorAll("[data-auth-mode]");

if (authForm && authTabs.length > 0) {
    let authMode = "login";
    const submitButton = authForm.querySelector(".auth-submit");
    const message = authForm.querySelector(".auth-message");
    const passwordInput = authForm.querySelector('input[name="password"]');

    const setAuthMode = (mode) => {
        authMode = mode;
        authTabs.forEach((tab) => {
            tab.classList.toggle("is-active", tab.dataset.authMode === mode);
        });
        if (submitButton) {
            submitButton.textContent = mode === "login" ? "登录" : "注册账号";
        }
        if (passwordInput) {
            passwordInput.autocomplete = mode === "login" ? "current-password" : "new-password";
        }
        if (message) {
            message.textContent = "";
            message.className = "auth-message";
        }
    };

    authTabs.forEach((tab) => {
        tab.addEventListener("click", () => setAuthMode(tab.dataset.authMode));
    });

    authForm.addEventListener("submit", async (event) => {
        event.preventDefault();

        if (message) {
            message.textContent = authMode === "login" ? "正在登录..." : "正在注册...";
            message.className = "auth-message";
        }
        if (submitButton) {
            submitButton.disabled = true;
        }

        try {
            const formData = new FormData(authForm);
            const response = await fetch(authMode === "login" ? "/api/login" : "/api/register", {
                method: "POST",
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8"
                },
                body: new URLSearchParams(formData)
            });
            const result = await response.json();

            if (message) {
                message.textContent = result.message || (result.ok ? "操作成功" : "操作失败");
                message.classList.toggle("is-success", Boolean(result.ok));
                message.classList.toggle("is-error", !result.ok);
            }

            if (result.ok && authMode === "login") {
                window.localStorage.setItem("blog_username", String(formData.get("username") || ""));
                window.localStorage.setItem("blog_token", result.token || "");
                updateAuthNav();
                window.location.href = "/";
            }
            if (result.ok && authMode === "register") {
                setAuthMode("login");
                if (message) {
                    message.textContent = result.message || "注册成功，可以登录了";
                    message.classList.add("is-success");
                }
            }
        }
        catch (error) {
            if (message) {
                message.textContent = "请求失败，请确认服务端正在运行";
                message.classList.add("is-error");
            }
        }
        finally {
            if (submitButton) {
                submitButton.disabled = false;
            }
        }
    });

    setAuthMode("login");
}

const commentRoot = document.querySelector("[data-comments]");

if (commentRoot) {
    const articleId = new URLSearchParams(window.location.search).get("id") || "1";
    const token = window.localStorage.getItem("blog_token") || "";
    const username = window.localStorage.getItem("blog_username") || "";
    const loginPanel = commentRoot.querySelector("[data-comment-login]");
    const commentForm = commentRoot.querySelector("[data-comment-form]");
    const commentList = commentRoot.querySelector("[data-comment-list]");
    const commentUser = commentRoot.querySelector("[data-comment-user]");
    const commentMessage = commentRoot.querySelector("[data-comment-message]");
    const textarea = commentForm ? commentForm.querySelector("textarea") : null;

    const setCommentMessage = (text, type) => {
        if (!commentMessage) {
            return;
        }
        commentMessage.textContent = text;
        commentMessage.className = `comment-message ${type ? `is-${type}` : ""}`.trim();
    };

    const renderComments = (comments) => {
        if (!commentList) {
            return;
        }

        commentList.innerHTML = "";
        if (comments.length === 0) {
            const empty = document.createElement("p");
            empty.className = "comment-empty";
            empty.textContent = "暂时还没有评论，来写第一条吧。";
            commentList.append(empty);
            return;
        }

        comments.forEach((comment) => {
            const item = document.createElement("article");
            item.className = "comment-item";

            const meta = document.createElement("div");
            meta.className = "comment-meta";

            const author = document.createElement("strong");
            author.textContent = comment.username || "用户";

            const time = document.createElement("span");
            time.textContent = comment.created_at || "";

            const content = document.createElement("p");
            content.textContent = comment.content || "";

            meta.append(author, time);
            item.append(meta, content);
            commentList.append(item);
        });
    };

    const showLoggedOutComments = (text) => {
        window.localStorage.removeItem("blog_token");
        window.localStorage.removeItem("blog_username");
        updateAuthNav();
        if (loginPanel) {
            loginPanel.hidden = false;
            const loginText = loginPanel.querySelector("p");
            if (loginText && text) {
                loginText.textContent = text;
            }
        }
        if (commentForm) {
            commentForm.hidden = true;
        }
        if (commentList) {
            commentList.hidden = true;
        }
        if (commentUser) {
            commentUser.textContent = "";
        }
    };

    const requestComments = async () => {
        const response = await fetch("/api/comments/list", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8"
            },
            body: new URLSearchParams({ token, article_id: articleId })
        });
        const result = await response.json();
        if (!result.ok) {
            if (response.status === 401) {
                showLoggedOutComments(result.message || "登录已过期，请重新登录。");
            }
            throw new Error(result.message || "评论加载失败");
        }
        renderComments(result.comments || []);
    };

    if (!token || !username) {
        if (loginPanel) {
            loginPanel.hidden = false;
        }
    }
    else {
        if (loginPanel) {
            loginPanel.hidden = true;
        }
        if (commentForm) {
            commentForm.hidden = false;
        }
        if (commentList) {
            commentList.hidden = false;
        }
        if (commentUser) {
            commentUser.textContent = `当前用户：${username}`;
        }

        requestComments().catch((error) => {
            setCommentMessage(error.message, "error");
        });
    }

    if (commentForm && textarea) {
        commentForm.addEventListener("submit", async (event) => {
            event.preventDefault();
            const content = textarea.value.trim();
            if (!content) {
                setCommentMessage("请先输入评论内容", "error");
                return;
            }

            setCommentMessage("正在发布...", "");
            const submitButton = commentForm.querySelector("button");
            if (submitButton) {
                submitButton.disabled = true;
            }

            try {
                const response = await fetch("/api/comments/add", {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8"
                    },
                    body: new URLSearchParams({ token, article_id: articleId, content })
                });
                const result = await response.json();
                if (!result.ok) {
                    if (response.status === 401) {
                        showLoggedOutComments(result.message || "登录已过期，请重新登录。");
                    }
                    throw new Error(result.message || "评论发布失败");
                }
                textarea.value = "";
                setCommentMessage(result.message || "评论已发布", "success");
                await requestComments();
            }
            catch (error) {
                setCommentMessage(error.message || "评论发布失败", "error");
            }
            finally {
                if (submitButton) {
                    submitButton.disabled = false;
                }
            }
        });
    }
}
