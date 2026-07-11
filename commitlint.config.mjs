export default {
    extends: ['@commitlint/config-conventional'],
    rules: {
        'type-enum': [2, 'always', ['build', 'chore', 'ci', 'feat', 'fix', 'test']],
        'scope-empty': [0, 'always']
    }
}
