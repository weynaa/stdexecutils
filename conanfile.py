from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.files import update_conandata
from conan.tools.scm import Git


class StdexecutilsConan(ConanFile):
    name = "stdexecutils"

    # Optional metadata
    license = "<Put the package license here>"
    author = "mwechner"
    url = "https://github.com/weynaa/stdexecutils"
    description = "Utilities extending the P2300 senders"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_qt": [True, False]}
    default_options = {"with_qt": False}

    # generators
    generators = ["CMakeDeps"]

    # Add dependencies here
    def requirements(self):
        self.requires("stdexec/2024.09", visible=True)
        if(self.options.with_qt):
           self.requires("qt/[>=6.7]", visible=True)

    # Add dependencies for building the package here
    def build_requirements(self):
        self.test_requires("gtest/[>=1.12.1]")

    def layout(self):
        cmake_layout(self)

    #Package version is the name of the git-tag, or development if we are on an untagged commit
    def set_version(self):
        git = Git(self, self.recipe_folder)
        try:
            self.version = git.run("describe --tags --exact-match")
        except:
            self.version = "development"

    def export(self):
        git = Git(self, self.recipe_folder)
        scm_url, scm_commit = git.get_url_and_commit()
        # we store the current url and commit in conandata.yml
        update_conandata(
            self, {"sources": {"commit": scm_commit, "url": scm_url}})

    def source(self):
        git = Git(self)
        sources = self.conan_data["sources"]
        git.clone(url=sources["url"], target=".")
        git.checkout(commit=sources["commit"])

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["CONAN_PACKAGE_NAME"] = self.name
        tc.cache_variables["CONAN_PACKAGE_VERSION"] = self.version
        tc.cache_variables["CONAN_PACKAGE_DESCRIPTION"] = self.description
        tc.cache_variables["CONAN_PACKAGE_URL"] = self.url
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test(env=['CTEST_OUTPUT_ON_FAILURE=1'])

    def package(self):
        cmake = CMake(self)
        cmake.install()
    
    def package_id(self):
        self.info.clear() # Header only

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
